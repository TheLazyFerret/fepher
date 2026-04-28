/// Source file of the TCP utils.

#include "./tcp_utils.hpp"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <expected>
#include <string>
#include <system_error>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define RETURN_UNEXPECTED_EC_ERRNO return std::unexpected(std::error_code(errno, std::generic_category()))

/// Set the chosen options to a socket that is not already binded.
std::expected<void, std::error_code> tcp_utils::set_socket_opts(int fd, SocketOption ops) noexcept {
  // Enable the fast reusing of the port after the program ends.
  if (ops.so_reuseaddr) {
    constexpr int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
      RETURN_UNEXPECTED_EC_ERRNO;
    }
  }
  // Max time for sending.
  if (ops.so_sndtimeo > 0) {
    const timeval time{.tv_sec = static_cast<time_t>(ops.so_sndtimeo), .tv_usec = 0};
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof(time)) < 0) {
      RETURN_UNEXPECTED_EC_ERRNO;
    }
  }
  // Max time for receiving
  if (ops.so_rcvtimeo > 0) {
    const timeval time{.tv_sec = static_cast<time_t>(ops.so_rcvtimeo), .tv_usec = 0};
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) < 0) {
      RETURN_UNEXPECTED_EC_ERRNO;
    }
  }
  // Set the socket to non blocking.
  if (ops.o_nonblock) {
    const int current_flags = fcntl(fd, F_GETFL, 0);
    if (current_flags < 0) {
      RETURN_UNEXPECTED_EC_ERRNO;
    }
    if (fcntl(fd, F_SETFL, (current_flags | O_NONBLOCK)) < 0) {
      RETURN_UNEXPECTED_EC_ERRNO;
    }
  }

  return {};
}

/// Convert an string IPv4 address into a net address.
std::expected<in_addr_t, std::error_code> tcp_utils::to_net_addr(const std::string& straddr) noexcept {
  in_addr addr;
  const int aux_result = inet_pton(AF_INET, straddr.c_str(), &addr);
  // In the very unlikely the protocol is incorrect.
  if (aux_result < 0) {
    RETURN_UNEXPECTED_EC_ERRNO;
  }
  // In the most likely of a bad formated address.
  if (aux_result == 0) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return {addr.s_addr};
}

/// Convert a net IPv4 address into a string address.
std::expected<std::string, std::error_code> tcp_utils::to_str_addr(in_addr_t inaddr) noexcept {
  char aux_dst[INET_ADDRSTRLEN];
  const in_addr aux_addr = {.s_addr = inaddr};
  if (inet_ntop(AF_INET, &aux_addr, aux_dst, sizeof(aux_dst)) == nullptr) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {aux_dst};
}

/// Return a ready-to-use sockaddr_in for IPv4.
std::expected<sockaddr_in, std::error_code> tcp_utils::make_sockaddr(
    std::uint32_t port, const std::string& addr) noexcept {
  const auto net_addr = to_net_addr(addr);
  if (!net_addr.has_value()) {
    return std::unexpected(net_addr.error());
  }
  sockaddr_in sockaddr;
  std::memset(&sockaddr, 0, sizeof(sockaddr)); // Set explicitly all bytes to 0.
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = net_addr.value();
  sockaddr.sin_port = htons(port);
  return {sockaddr};
}

std::expected<int, std::error_code> tcp_utils::create_listen_socket(
    std::uint32_t port, const std::string& addr, SocketOption ops, std::uint32_t backlog) noexcept {
  // Creates the sockaddr_in before anything else, So if it fails, it does quickly.
  const auto result_addr = tcp_utils::make_sockaddr(port, addr);
  if (!result_addr.has_value()) {
    return std::unexpected(result_addr.error());
  }
  // Create the socket
  const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    RETURN_UNEXPECTED_EC_ERRNO;
  }
  // Set the options.
  const auto result_set_sockops = tcp_utils::set_socket_opts(socket_fd, ops);
  if (!result_set_sockops.has_value()) {
    close(socket_fd);
    return std::unexpected(result_set_sockops.error());
  }
  // Bind the socket.
  if (bind(socket_fd, reinterpret_cast<const sockaddr*>(&result_addr.value()), sizeof(addr)) < 0) {
    close(socket_fd);
    RETURN_UNEXPECTED_EC_ERRNO;
  }
  // Marks the socket as a passive socket.
  if (listen(socket_fd, static_cast<int>(backlog)) < 0) {
    close(socket_fd);
    RETURN_UNEXPECTED_EC_ERRNO;
  }
  return socket_fd;
}

/// Attempts to send all the bytes in the src vector.
std::expected<void, std::error_code> tcp_utils::send_t(int fd, const std::vector<uint8_t>& src) noexcept {
  if (src.size() == 0) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  ssize_t bytes_sent = 0;
  while (bytes_sent < src.size()) {
    const ssize_t aux_bytes_sent = send(fd, src.data() + bytes_sent, src.size() - bytes_sent, 0);
    if (aux_bytes_sent < 0) {
      RETURN_UNEXPECTED_EC_ERRNO;
    } else if (aux_bytes_sent == 0) {
      return std::unexpected(std::make_error_code(std::errc::connection_reset));
    }
    bytes_sent += aux_bytes_sent;
  }
  return {};  
}

/// Attempts to receive at most len bytes from the connection.
/// Will return a vector with the size of the number of bytes received.
std::expected<std::vector<uint8_t>, std::error_code> tcp_utils::recv_t(int fd, std::size_t len) noexcept {
  std::vector<uint8_t> buffer(len, 0);
  const ssize_t bytes_rec = recv(fd, buffer.data(), len, 0);
  if (bytes_rec < 0) {
    RETURN_UNEXPECTED_EC_ERRNO;
  } else if (bytes_rec == 0) {
    return std::unexpected(std::make_error_code(std::errc::connection_reset));
  }
  buffer.resize(static_cast<std::size_t>(bytes_rec));
  return buffer;  
}