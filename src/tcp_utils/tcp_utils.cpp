/// Source file of the TCP utils.

#include "./tcp_utils.hpp"

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <ctime>
#include <expected>
#include <string>
#include <system_error>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

using namespace tcp_utils;

/// Set the chosen options to a socket that is not already binded.
std::expected<void, std::error_code> tcp_utils::set_socket_opts(socket_t fd, SocketOptions ops) noexcept {
  // Enable the fast reusing of the port after the program ends.
  if (ops.so_reuseaddr) {
    constexpr int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
      return std::unexpected(std::error_code(errno, std::generic_category()));
    }
  }
  // Max time for sending.
  if (ops.so_sndtimeo > 0) {
    const timeval time{.tv_sec = static_cast<time_t>(ops.so_sndtimeo), .tv_usec = 0};
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof(time)) < 0) {
      return std::unexpected(std::error_code(errno, std::generic_category()));
    }
  }
  // Max time for receiving
  if (ops.so_rcvtimeo > 0) {
    const timeval time{.tv_sec = static_cast<time_t>(ops.so_rcvtimeo), .tv_usec = 0};
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) < 0) {
      return std::unexpected(std::error_code(errno, std::generic_category()));
    }
  }
  // Set the socket to non blocking.
  if (ops.o_nonblock) {
    const int current_flags = fcntl(fd, F_GETFL, 0);
    if (current_flags < 0) {
      return std::unexpected(std::error_code(errno, std::generic_category()));
    }
    if (fcntl(fd, F_SETFL, (current_flags | O_NONBLOCK)) < 0) {
      return std::unexpected(std::error_code(errno, std::generic_category()));
    }
  }

  return {};
}

/// Return a ready-to-use sockaddr_in for IPv4.
/// Only should fail if the string addr is not a valid IPv4 address.
std::expected<sockaddr_in, std::error_code> tcp_utils::make_sockaddr(
    std::uint16_t port, const std::string& addr) noexcept {
  const auto net_addr = to_net_addr(addr);
  if (!net_addr.has_value()) {
    return std::unexpected(net_addr.error());
  }
  sockaddr_in sockaddr{};
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = net_addr.value();
  sockaddr.sin_port = htons(port);
  return {sockaddr};
}

/// Convert an string IPv4 address into a net address.
std::expected<in_addr_t, std::error_code> tcp_utils::to_net_addr(const std::string& straddr) noexcept {
  in_addr addr;
  const int aux_result = inet_pton(AF_INET, straddr.c_str(), &addr);
  // In the very unlikely the protocol is incorrect.
  // If this assertion fail, very probably is a programming error.
  assert(aux_result >= 0);
  // In the most likely of a bad formated address.
  if (aux_result == 0) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return {addr.s_addr};
}

/// Convert a net IPv4 address into a string address.
std::string tcp_utils::to_str_addr(in_addr_t inaddr) noexcept {
  char aux_dst[INET_ADDRSTRLEN];
  const in_addr aux_addr = {.s_addr = inaddr};
  // This should never fail. If it does, probably is a programming error.
  assert(inet_ntop(AF_INET, &aux_addr, aux_dst, sizeof(aux_dst)) != nullptr);
  return {aux_dst};
}

/// Return the local port and address a socket is connected to.
/// Only should fail if fd is not a valid socket.
std::expected<SocketAddress, std::error_code> tcp_utils::get_socket_local_addr(socket_t fd) noexcept {
  sockaddr_in aux_addr{}; // Initialize it (empty).
  socklen_t aux_addr_size = sizeof(aux_addr);
  if (getsockname(fd, reinterpret_cast<sockaddr*>(&aux_addr), &aux_addr_size) < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  const std::string addr = tcp_utils::to_str_addr(aux_addr.sin_addr.s_addr);
  const std::uint16_t port = ntohs(aux_addr.sin_port);
  return SocketAddress{addr, port};
}

/// Return the remote port and address a socket is connected to.
/// Should only fail if the fd is a not valid and connected socket.
std::expected<SocketAddress, std::error_code> tcp_utils::get_socket_remote_addr(socket_t fd) noexcept {
  sockaddr_in aux_addr{}; // Initialize it (empty).
  socklen_t aux_addr_size = sizeof(aux_addr);
  if (getpeername(fd, reinterpret_cast<sockaddr*>(&aux_addr), &aux_addr_size) < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  const std::string addr = tcp_utils::to_str_addr(aux_addr.sin_addr.s_addr);
  const std::uint16_t port = ntohs(aux_addr.sin_port);
  return SocketAddress{addr, port};
}

/// Create a listen, ready to connect TCP socket.
std::expected<int, std::error_code> tcp_utils::create_listen_socket(sockaddr_in addr, std::uint32_t backlog) noexcept {
  // Create the socket
  const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  // Bind the socket.
  if (bind(socket_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(socket_fd);
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  // Marks the socket as a passive socket.
  if (listen(socket_fd, static_cast<int>(backlog)) < 0) {
    close(socket_fd);
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {socket_fd};
}

/// -- All this functions are for checking values.

/// Check if the error is the kind of "connection closed".
bool tcp_utils::connection_closed(int error) noexcept {
  switch (error) {
  case ECONNABORTED:
  case EPIPE:
  case ECONNRESET:
  case ENOTCONN:
  case ETIMEDOUT:
    return true;
  default:
    return false;
  }
}

/// Check if the parameter (should be errno) is of the type try again later (in a future call).
bool tcp_utils::try_again_later(int error) noexcept {
  if (error == EAGAIN || error == EWOULDBLOCK) {
    return true;
  }
  return false;
}

/// Check if the parameter (should be rrno) is EINTR, and should try again.
bool tcp_utils::try_again(int error) noexcept {
  if (error == EINTR) {
    return true;
  }
  return false;
}
