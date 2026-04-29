/// Gopher server implementation.

#include "./server.hpp"
#include "../common/common.hpp"

#include <expected>
#include <format>
#include <string_view>
#include <system_error>

#include <sys/epoll.h>
#include <unistd.h>

using namespace server;
using namespace tcp_utils;

/// Close all the inner file descriptors.
void GopherServer::destroy() noexcept {
  std::vector<socket_t> open_connections;
  // We move all the connections to an auxiliar vector, so not modyfing of the map while go through it.
  for (auto& conn : connections) {
    open_connections.push_back(conn.first);
  }
  for (auto& conn : open_connections) {
    // Close all the connections, ignoring possible errors.
    close_connection(conn).value();
  }
  // Close the listen socket.
  close(this->listen_fd);
  // Close the epoll file descriptor.
  close(epoll_fd);
}

/// Attempts to remove a socket from the epoll.
std::expected<void, std::error_code> GopherServer::close_connection(tcp_utils::socket_t socket) noexcept {
  // Check the argument is in the events.
  if (this->connections.find(socket) == this->connections.end()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  // Second we remove it from the connections map.
  this->connections.erase(socket);
  // Now we close the socket.
  // It is optional remove it explicitly from the epoll.
  close(socket);
  return {};
}

/// Attempts to add a socket in the epoll.
std::expected<void, std::error_code> GopherServer::add_connection(tcp_utils::socket_t socket) noexcept {
  // The socket is already being watched.
  if (this->connections.find(socket) != this->connections.end()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
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
  // Check the argument is in the events.
  if (this->connections.find(socket) == this->connections.end()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  struct epoll_event event{};
  event.data.fd = socket;
  event.events = EPOLLOUT | EPOLLET; // Only output (and edge trigered).
  if (epoll_ctl(this->epoll_fd, EPOLL_CTL_MOD, socket, &event) < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {};
}

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

/// Build a new server instance.
std::expected<GopherServer, std::error_code> GopherServer::build(std::string_view addr, std::uint16_t port) noexcept {
  GopherServer server;
  /// Create the listen socket.
  const SocketOptions listen_ops{.so_reuseaddr = true, .o_nonblock = true};
  const auto listen_result = tcp_utils::create_listen_socket(port, std::string(addr), listen_ops, BAKCLOG);
  if (!listen_result) {
    common::print_error("Error creating server");
    return std::unexpected(listen_result.error());
  }
  server.listen_fd = listen_result.value();
  /// Create the epoll.
  const auto epoll_result = GopherServer::create_epoll();
  if (!epoll_result) {
    close(server.listen_fd);
    common::print_error("Error creating server");
    return std::unexpected(epoll_result.error());
  }
  server.epoll_fd = epoll_result.value();
  /// Add the listen socket.
  struct epoll_event listen_ev{};
  listen_ev.events = EPOLLIN | EPOLLET; // Input and edge trigered.
  listen_ev.data.fd = server.listen_fd;
  if (epoll_ctl(server.epoll_fd, EPOLL_CTL_ADD, server.listen_fd, &listen_ev) < 0) {
    close(server.listen_fd);
    close(server.epoll_fd);
    common::print_error("Error creating server");
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  // Should never fail.
  const auto address = tcp_utils::get_socket_local_addr(server.listen_fd).value();
  common::print_info(std::format("Server instance created listening to {} at {}", address.address, address.port));

  return {std::move(server)};
}

/// Small wrapper for creating an empty epoll.
std::expected<epoll_t, std::error_code> GopherServer::create_epoll() noexcept {
  epoll_t epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {epoll_fd};
}
