/// Functions used by the tcp socket abstraction.

#pragma once

#include "./tcp.hpp"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <expected>
#include <system_error>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

namespace tcp_common {

/// Auxiliar method for setting all set options in the SocketOption struct
inline std::expected<void, std::error_code> set_socket_options(int fd, SocketOption ops) noexcept {
  /// Enable the fast reusing of the port after the program closes.
  if (ops.so_reuseaddr) {
    constexpr int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
      return std::unexpected(std::error_code(errno, std::generic_category()));
    }
  }
  /// Max time for sending.
  if (ops.so_sndtimeo > 0) {
    const timeval time{.tv_sec = static_cast<time_t>(ops.so_sndtimeo), .tv_usec = 0};
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof(time)) < 0) {
      return std::unexpected(std::error_code(errno, std::generic_category()));
    }
  }
  /// Max time for receiving.
  if (ops.so_rcvtimeo > 0) {
    const timeval time{.tv_sec = static_cast<time_t>(ops.so_rcvtimeo), .tv_usec = 0};
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) < 0) {
      return std::unexpected(std::error_code(errno, std::generic_category()));
    }
  }
  return {};
}

/// Convert an string IPv4 address into a net address.
inline std::expected<in_addr_t, std::error_code> to_net_addr(const std::string& straddr) noexcept {
  in_addr addr;
  const int aux_result = inet_pton(AF_INET, straddr.c_str(), &addr);
  // In the very unlikely the protocol is incorrect.
  if (aux_result < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  // In the most likely of a bad formated address.
  if (aux_result == 0) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return {addr.s_addr};
}

/// Convert a net IPv4 address into a string address.
inline std::expected<std::string, std::error_code> to_str_addr(in_addr_t inaddr) noexcept {
  char aux_dst[INET_ADDRSTRLEN];
  const in_addr aux_addr = {.s_addr = inaddr};
  if (inet_ntop(AF_INET, &aux_addr, aux_dst, sizeof(aux_dst)) == nullptr) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {aux_dst};
}

/// Return a ready-to-use sockaddr_in for IPv4.
inline std::expected<sockaddr_in, std::error_code> make_sockaddr(std::uint32_t port, const std::string& addr) noexcept {
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

} // namespace tcp_common
