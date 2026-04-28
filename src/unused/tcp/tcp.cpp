/// Source file of the TCP socket abstraction.

#include "./tcp.hpp"
#include "./common.hpp"

#include <cerrno>
#include <cstdint>
#include <expected>
#include <string>
#include <system_error>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>
#include <utility>

using namespace tcp_common;

/// Move constructor.
TcpListener::TcpListener(TcpListener&& e) noexcept {
  this->socket_fd = e.socket_fd;
  e.socket_fd = -1;
}

/// Move assignment.
TcpListener& TcpListener::operator=(TcpListener&& e) noexcept {
  destroy(); // if there is already a socket active, close it first.
  this->socket_fd = e.socket_fd;
  e.socket_fd = -1;
  return *this;
}

/// Returns an instance of a TCPListener, or an error if something bad happens.
std::expected<TcpListener, std::error_code> TcpListener::build(
    std::uint16_t port, const std::string& str_addr, SocketOption ops, std::uint16_t backlog) noexcept {
  // Creates the sockaddr_in before anything else, So if it fails, it does quickly.
  const auto result_addr = make_sockaddr(port, str_addr);
  if (!result_addr.has_value()) {
    return std::unexpected(result_addr.error());
  }
  const auto addr = result_addr.value();
  // Creates the socket.
  const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  // Set the options.
  const auto result_set_socket_options = set_socket_options(socket_fd, ops);
  if (!result_set_socket_options.has_value()) {
    close(socket_fd); // Avoid resorce leak.
    return std::unexpected(result_set_socket_options.error());
  }
  // Bind the socket to a port.
  if (bind(socket_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(socket_fd);
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  // Marks the socket as a passive socket.
  if (listen(socket_fd, static_cast<int>(backlog)) < 0) {
    close(socket_fd);
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return TcpListener(socket_fd);
}

/// Close the inner file descriptor.
void TcpListener::destroy() noexcept {
  if (this->socket_fd >= 0) {
    close(this->socket_fd);
    this->socket_fd = -1;
  }
}

/// Blocks the current thread until there is a new avaible connection.
std::expected<TcpConnection, std::error_code> TcpListener::accept_connection() const noexcept {
  sockaddr_in addr{};
  socklen_t addr_size = sizeof(addr);
  const int socket_fd = accept(this->socket_fd, reinterpret_cast<sockaddr*>(&addr), &addr_size);
  if (socket_fd < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  // This conversion should be correct, therefore should never fail.
  const std::string str_addr = to_str_addr(addr.sin_addr.s_addr).value();
  const std::uint16_t port = ntohs(addr.sin_port);
  return TcpConnection{socket_fd, str_addr, port};
}

/// Move constructor.
TcpConnection::TcpConnection(TcpConnection&& e) noexcept {
  this->socket_fd = e.socket_fd;
  this->str_addr = e.str_addr;
  this->port = e.port;
  e.socket_fd = -1;
  e.str_addr = "";
  e.port = 0;
}

/// Move assigment.
TcpConnection& TcpConnection::operator=(TcpConnection&& e) noexcept {
  destroy();
  this->socket_fd = e.socket_fd;
  this->str_addr = e.str_addr;
  this->port = e.port;
  e.socket_fd = -1;
  e.str_addr = "";
  e.port = 0;
  return *this;
}

/// Return the address and the port the socket is connected.
std::pair<std::string, std::uint16_t> TcpConnection::get_address() const noexcept {
  return std::make_pair(str_addr, port);
}

/// Close the inner file descriptor.
void TcpConnection::destroy() noexcept {
  if (this->socket_fd >= 0) {
    close(this->socket_fd);
    this->socket_fd = -1;
  }
}

/// Attempts to send all the bytes in the src vector.
std::expected<void, std::error_code> TcpConnection::send_t(const std::vector<uint8_t>& src) const noexcept {
  if (src.size() == 0) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  ssize_t bytes_sent = 0;
  while (bytes_sent < src.size()) {
    const ssize_t aux_bytes_sent = send(this->socket_fd, src.data() + bytes_sent, src.size() - bytes_sent, 0);
    if (aux_bytes_sent < 0) {
      return std::unexpected(std::error_code(errno, std::generic_category()));
    } else if (aux_bytes_sent == 0) {
      return std::unexpected(std::make_error_code(std::errc::connection_reset));
    }
    bytes_sent += aux_bytes_sent;
  }
  return {};
}

/// Attempts to receive at most len bytes from the connection.
/// Will return a vector with the size of the number of bytes received.
std::expected<std::vector<uint8_t>, std::error_code> TcpConnection::recv_t(std::size_t len) const noexcept {
  std::vector<uint8_t> buffer(len, 0);
  const ssize_t bytes_rec = recv(this->socket_fd, buffer.data(), len, 0);
  if (bytes_rec < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  } else if (bytes_rec == 0) {
    return std::unexpected(std::make_error_code(std::errc::connection_reset));
  }
  buffer.resize(static_cast<std::size_t>(bytes_rec));
  return buffer;
}
