/// Gopher server implementation.

#include "./server.hpp"
#include "../common/common.hpp"
// #include "../gopher/gopher.hpp"

#include <cerrno>
#include <expected>
#include <format>
// #include <string_view>
#include <cassert>
#include <sys/ucontext.h>
#include <system_error>

#include <sys/epoll.h>
#include <unistd.h>

using namespace server;
using namespace tcp_utils;

/// -- BASIC CLASS STUFF

/// Move constructor.
GopherServer::GopherServer(GopherServer&& e) noexcept {
  this->listen_fd = e.listen_fd;
  e.listen_fd = -1;
  this->epoll_fd = e.epoll_fd;
  e.epoll_fd = -1;
  this->connections = std::move(e.connections);
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
  return *this;
}

/// Close all the inner file descriptors.
void GopherServer::destroy() noexcept {
  std::vector<socket_t> open_connections;
  // We move all the connections to an auxiliar vector, so not modyfing of the map while go through it.
  for (auto& conn : connections) {
    open_connections.push_back(conn.first);
  }
  for (auto& conn : open_connections) {
    // Close all the connections, ignoring possible errors.
    close_connection(conn);
  }
  // Close the listen socket.
  close(this->listen_fd);
  // Close the epoll file descriptor.
  close(epoll_fd);
}

/// Build a new server instance.
std::expected<GopherServer, std::error_code> GopherServer::build(sockaddr_in addr) noexcept {
  GopherServer server;
  // Create the listening socket.
  const auto listenfd_result = tcp_utils::create_listen_socket(addr, BAKCLOG);
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
    return std::unexpected(std::error_code(errno, std::generic_category()));
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
std::expected<void, std::error_code> GopherServer::add_connection(socket_t socket) noexcept {
  // Check if the socket is not already being watched.
  assert(this->connections.find(socket) == this->connections.end());
  // Add it in the connections map.
  this->connections.insert({socket, {}});
  // Add it in the epoll.
  struct epoll_event event{};
  event.data.fd = socket;
  event.events = EPOLLIN | EPOLLET; // At the begin, all connections expect input. Edge trigered.
  if (epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, socket, &event) < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {};
}

/// Switch the connection from EPOLLIN to EPOLLOUT.
/// Although its name say switch, is only one way.
std::expected<void, std::error_code> GopherServer::switchout_connection(tcp_utils::socket_t socket) noexcept {
  // Check the connection is being watched.
  assert(this->connections.find(socket) != this->connections.end());
  struct epoll_event event{};
  event.data.fd = socket;
  event.events = EPOLLOUT | EPOLLET; // Only output (and edge trigered).
  if (epoll_ctl(this->epoll_fd, EPOLL_CTL_MOD, socket, &event) < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {};
}

/// Attempts to remove a socket from the epoll.
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
    const int nfds = epoll_wait(this->epoll_fd, events, MAX_EVENTS, -1);
    if (nfds < 0) {
      const int errno_value = errno;
      common::print_error("Error calling epoll_wait");
      return std::unexpected(std::error_code(errno_value, std::generic_category()));
    }
    for (int n = 0; n < nfds; ++n) {
      if (events[n].data.fd == this->listen_fd) {
        if (const auto e = handle_incomming_connection(); !e) {
          common::print_error("Fatal error handling incomming connection");
          return std::unexpected(e.error());
        }
      } else {
        if (const auto e = handle_connection(events[n].data.fd); !e) {
          common::print_error("Fatal error handling connection");
          return std::unexpected(e.error());
        }
      }
    }
  }
  return {};
}

/// Auxiliar function of run, handles a single connection.
std::expected<void, std::error_code> GopherServer::handle_connection(socket_t fd) noexcept { return {}; }

/// Auxiliar function of run, handle a single incoming connection.
std::expected<void, std::error_code> GopherServer::handle_incomming_connection() noexcept { return {}; }

/// Recv function nonblocking wrapper.
/// Grab all the data currently avaible in the socket.
std::expected<void, std::error_code> GopherServer::connection_recv(socket_t fd, ConnectionStatus& st) noexcept {
  auto& buffer = st.receive_buffer;
  auto& buffer_index = st.receive_index;

  return {};
}
