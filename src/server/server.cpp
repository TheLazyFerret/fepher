/// Gopher server implementation.

#include "./server.hpp"

#include <string_view>
#include <expected>
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
  this->connections.insert(socket, {});
  // Add it in the epoll.
  struct epoll_event event {};
  event.data.fd = socket;
  event.events = EPOLLIN; // At the begin, all connections expect input.
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
  struct epoll_event event {};
  event.data.fd = socket;
  event.events = EPOLLOUT; // Only output.
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