/// Gopher server implementation.

#include "./server.hpp"
#include "../common/common.hpp"
#include "../gopher/gopher.hpp"

#include <cassert>
#include <cerrno>
#include <ctime>
#include <expected>
#include <format>
#include <sys/time.h>
#include <system_error>

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

using namespace server;
using namespace tcp_utils;
using namespace gopher;

/// -- BASIC CLASS STUFF

/// Move constructor.
GopherServer::GopherServer(GopherServer&& e) noexcept {
  this->listen_fd = e.listen_fd;
  e.listen_fd = -1;
  this->epoll_fd = e.epoll_fd;
  e.epoll_fd = -1;
  this->connections = std::move(e.connections);
  this->timers = std::move(e.timers);
}

/// Move assignment.
GopherServer& GopherServer::operator=(GopherServer&& e) noexcept {
  // Self assigmnet protection.
  if (this == &e) {
    return *this;
  }
  this->destroy();
  this->listen_fd = e.listen_fd;
  e.listen_fd = -1;
  this->epoll_fd = e.epoll_fd;
  e.epoll_fd = -1;
  this->connections = std::move(e.connections);
  this->timers = std::move(e.timers);
  return *this;
}

/// Close all the inner file descriptors.
void GopherServer::destroy() noexcept {
  // Close all the timers.
  std::vector<timer_t> pending_timers; // List of open timer file descriptor.
  for (auto& timer : this->timers) {
    pending_timers.push_back(timer.first);
  }
  for (auto& timer : pending_timers) {
    close_timer(timer);
  }
  std::vector<socket_t> open_connections; // List of open connections file descriptor.
  // We move all the connections to an auxiliar vector, so not modyfing of the map while go through it.
  for (auto& conn : this->connections) {
    open_connections.push_back(conn.first);
  }
  for (auto& conn : open_connections) {
    // Close all the connections, ignoring possible errors.
    close_connection(conn);
  }
  // Close the listen socket.
  close_listen_socket();
  // Close the epoll file descriptor.
  if (epoll_fd >= 0) {
    close(epoll_fd);
  }
}

/// Build a new server instance.
std::expected<GopherServer, std::error_code> GopherServer::build(sockaddr_in addr) noexcept {
  GopherServer server;
  // Create the listening socket.
  const auto listenfd_result = tcp_utils::create_listen_socket(addr, BACKLOG);
  if (!listenfd_result) {
    common::print_error("Failed to create listening socket");
    return std::unexpected(listenfd_result.error()); // Fordward the error.
  }
  const int listen_fd = listenfd_result.value();
  // Set the listening socket to nonblocking.
  if (const auto e = tcp_utils::set_socket_opts(listen_fd, {.o_nonblock = true}); !e.has_value()) {
    common::print_error("Failed to set the listening socket to nonblocking");
    close(listen_fd);                  // Close the previous created listening socket.
    return std::unexpected(e.error()); // Fordward the error.
  }
  // Create the epoll file descriptor.
  const auto epollfd_result = GopherServer::create_epoll();
  if (!epollfd_result) {
    common::print_error("Failed to create epoll file descriptor");
    close(listen_fd);                               // Close the previous created listening socket.
    return std::unexpected(epollfd_result.error()); // Fordward the error.
  }
  const int epoll_fd = epollfd_result.value();
  // Insert the listen socket into the epoll.
  struct epoll_event listen_event{};
  listen_event.data.fd = listen_fd;
  listen_event.events = EPOLLIN | EPOLLET;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &listen_event) < 0) {
    common::print_error("Failed to add the listen socket into the epoll");
    const int errno_value = errno;
    close(listen_fd);
    close(epoll_fd);
    return std::unexpected(std::error_code(errno_value, std::generic_category()));
  }
  // Finishing creating the structure and return it.
  server.epoll_fd = epoll_fd;
  server.listen_fd = listen_fd;
  common::print_info(
      std::format("Server opened in port: {}", tcp_utils::get_socket_local_addr(listen_fd).value().port));
  return {std::move(server)};
}

/// -- EPOLL MODIFIERS.

/// Attempts to add a socket in the epoll.
/// If it fails, it attempts to not modify the status.
std::expected<void, std::error_code> GopherServer::add_connection(socket_t socket) noexcept {
  // Check if the socket is not already being watched.
  assert(this->connections.find(socket) == this->connections.end());
  // Add it in the epoll first.
  struct epoll_event socket_event{};
  socket_event.data.fd = socket;
  socket_event.events = EPOLLIN | EPOLLET; // At the begin, all connections expect input. Edge trigered.
  if (epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, socket, &socket_event) < 0) {
    const int errno_value = errno;
    close(socket);
    return std::unexpected(std::error_code(errno_value, std::generic_category()));
  }
  // Creates the timer.
  const timer_t timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  if (timer_fd < 0) {
    const int errno_value = errno;
    close(socket);
    return std::unexpected(std::error_code(errno_value, std::generic_category()));
  }
  // Set the values of the timer.
  struct itimerspec timer_specs{};
  timer_specs.it_value.tv_sec = static_cast<time_t>(CONNECTION_TIMEOUT);
  if (timerfd_settime(timer_fd, 0, &timer_specs, nullptr) < 0) {
    const int errno_value = errno;
    close(socket);
    close(timer_fd);
    return std::unexpected(std::error_code(errno_value, std::generic_category()));
  }
  // Add the timer to the epoll.
  struct epoll_event timer_event{};
  timer_event.data.fd = timer_fd;
  timer_event.events = EPOLLIN;
  if (epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, timer_fd, &timer_event) < 0) {
    const int errno_value = errno;
    close(socket);
    close(timer_fd);
    return std::unexpected(std::error_code(errno_value, std::generic_category()));
  }
  // Add the connection to the connections map.
  struct ConnectionStatus aux{};
  aux.timer = timer_fd;
  this->connections.insert({socket, std::move(aux)});
  // Add the timer to the timer map.
  this->timers.insert({timer_fd, socket});
  return {};
}

/// Switch the connection mode between EPOLLIN and EPOLLOUT (+ always EPOLLET).
std::expected<void, std::error_code> GopherServer::switch_connection_mode(socket_t socket, EpollMode mode) noexcept {
  // Check the connection is being watched.
  assert(this->connections.find(socket) != this->connections.end());
  struct epoll_event event{};
  event.data.fd = socket;
  if (mode == EpollMode::Epollin) {
    event.events = EPOLLIN | EPOLLET; // Only output (and edge trigered).
  } else if (mode == EpollMode::Epollout) {
    event.events = EPOLLOUT | EPOLLET; // Only output (and edge trigered).
  }
  if (epoll_ctl(this->epoll_fd, EPOLL_CTL_MOD, socket, &event) < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {};
}

/// Remove a connection socket from the epoll and the map.
void GopherServer::close_connection(tcp_utils::socket_t socket) noexcept {
  // Check the connection is being watched.
  // If it is not, it is a programming error (my fault xD).
  assert(this->connections.find(socket) != this->connections.end());
  // Second we remove it from the connections map.
  this->connections.erase(socket);
  // Now we close the socket. If it fails it is what it is.
  // It is optional remove it explicitly from the epoll.
  close(socket);
}

/// Remove a timer file descriptor from the epoll and the map.
void GopherServer::close_timer(timer_t timer) noexcept {
  // Check the timer exist in the timer map.
  // If doesn't, programming error.
  assert(this->timers.find(timer) != this->timers.end());
  // Remove it from the map.
  this->timers.erase(timer);
  // Close the file descriptor and the remove it from the epoll.
  close(timer);
}

/// Small wrapper for creating an empty epoll.
std::expected<epoll_t, std::error_code> GopherServer::create_epoll() noexcept {
  epoll_t epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {epoll_fd};
}

/// - EXECUTION METHODS

/// Main function.
std::expected<void, std::error_code> GopherServer::run() noexcept {
  epoll_event events[MAX_EVENTS]{};
  while (true) {
    const int nfds = epoll_wait(this->epoll_fd, events, MAX_EVENTS, EPOLL_WAIT_TIME);
    const int errno_value = errno;
    /// Check if the stop signal has been called.
    if (!common::run) {
      if (this->listen_fd >= 0) {
        common::print_info("Server has received the stop signal.");
        close_listen_socket();
        common::print_info("Stopped incoming connections.");
        common::print_info("Finishing pending connections.");
      }
      if (this->connections.empty()) {
        break;
      }
    }
    /// Check possible errors.
    if (nfds < 0) {
      /// Signal interruption, try again.
      if (tcp_utils::try_again(errno_value)) {
        common::print_warning("Signal interrupted epoll_wait");
        continue;
      }
      /// Fatal error.
      common::print_error("Fatal error calling epoll_wait");
      return std::unexpected(std::error_code(errno_value, std::generic_category()));
    }
    /// Check all events.
    for (int n = 0; n < nfds; ++n) {
      const int current_fd = events[n].data.fd;
      if (current_fd == this->listen_fd) {
        handle_incomming_connection();
      } else if (this->connections.find(current_fd) != this->connections.end()) {
        handle_connection(events[n].data.fd);
      } else if (this->timers.find(current_fd) != this->timers.end()) {
        // send a connection timeout.
        common::print_warning(std::format("Connection timeout for fd: {}", this->timers[current_fd]));
        close_connection(this->timers[current_fd]);
        close_timer(current_fd);
      }
    }
  }
  return {};
}

/// Close the inner listen socket, implicitly removing it from the epoll.
void GopherServer::close_listen_socket() noexcept {
  if (this->listen_fd >= 0) {
    close(this->listen_fd);
    this->listen_fd = -1;
  }
}

/// Auxiliar function of run(), handle a single incoming connection.
void GopherServer::handle_incomming_connection() noexcept {
  // Attempt to accept the incoming connect
  while (true) {
    const socket_t new_conn = accept(this->listen_fd, nullptr, nullptr);
    if (new_conn < 0) {
      if (tcp_utils::try_again(errno)) {
        continue;
      } else if (tcp_utils::try_again_later(errno)) {
        break;
      }
      common::print_warning("Failed to accept incoming connection");
      return;
    }
    // Make the incoming socket nonblocking.
    if (!tcp_utils::set_socket_opts(new_conn, {.o_nonblock = true})) {
      close(new_conn);
      common::print_warning("Failed to put the incoming socket to nonblocking");
      continue; // Ignore this connection and continue with the rest.
    }
    // Add the connection.
    // If it fails, the call closes the socket.
    if (!add_connection(new_conn)) {
      common::print_warning("Failed to add the incoming connection.");
      continue; // Ignore this connection and continue with the rest.
    }
    common::print_info(std::format("New incoming connection with fd: {}", new_conn));
  }
}

/// Auxiliar function of run(), handles a single connection.
/// Only return an error in case of a fatal error.
void GopherServer::handle_connection(socket_t fd) noexcept {
  // If this assert fail, it is a programming error.
  assert(this->connections.find(fd) != connections.end());
  ConnectionStatus& cs = this->connections[fd];
  // Receiving phase.
  if (!cs.receiving_finished) {
    if (!connection_recv(fd, cs)) {
      // Fatal connection error.
      common::print_warning("Error calling recv. Connection closed.");
      close_timer(cs.timer);
      close_connection(fd);
      return;
    }
    // Check received data.
    if (cs.receive_index >= cs.receive_buffer.size()) {
      // Maximum buffer size exceeded.
      common::print_warning("Connection exceeded maximum buffer size. Connection closed.");
      close_timer(cs.timer);
      close_connection(fd);
      return;
    }
    const std::string buffer_str(cs.receive_buffer.begin(), cs.receive_buffer.begin() + cs.receive_index);
    if (gopher::string_end_in_terminator(buffer_str)) {
      // Finished receiving phase.
      if (!switch_connection_mode(fd, EpollMode::Epollout)) {
        common::print_warning("Failed to change connection epoll mode. Connection closed.");
        close_timer(cs.timer);
        close_connection(fd);
        return;
      }
      cs.receiving_finished = true;
      // Close the timer for receiving finished connections.
      close_timer(cs.timer);
      cs.timer = -1;
    }
    return;
  }
  // Sending phase.
  // Temporal
  const std::string a = "PERRO\n";
  send(fd, a.data(), a.size(), 0);
  close_connection(fd);
  // todo!
}

/// Recv function nonblocking wrapper.
/// Grab all the data currently avaible in the socket.
std::expected<void, std::error_code> GopherServer::connection_recv(socket_t fd, ConnectionStatus& st) noexcept {
  auto& buffer = st.receive_buffer;
  auto& buffer_index = st.receive_index;
  while (buffer_index < buffer.size()) {
    const ssize_t bytes_received = recv(fd, buffer.data() + buffer_index, buffer.size() - buffer_index, 0);
    // Normal bytes received.
    if (bytes_received > 0) {
      buffer_index += static_cast<std::size_t>(bytes_received);
      continue;
    }
    // Repeat immediately.
    else if (bytes_received < 0 && tcp_utils::try_again(errno)) {
      continue;
    }
    // Repeat later (for nonblocking socket).
    else if (bytes_received < 0 && tcp_utils::try_again_later(errno)) {
      break;
    }
    // Uncleanly closed connection.
    else if (bytes_received == 0) {
      return std::unexpected(std::make_error_code(std::errc::connection_aborted));
    }
    // Closed connection or other weird error.
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {};
}
