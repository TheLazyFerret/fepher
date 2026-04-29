/// Gopher server.

#pragma once

#include "../tcp_utils/tcp_utils.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace server {

using epoll_t = int;

constexpr std::size_t MAX_MESSAGE_LENGTH = 1024;
constexpr std::size_t BAKCLOG = 10;

/// Represents each connection and its state.
struct ConnectionStatus {
  std::array<std::uint8_t, MAX_MESSAGE_LENGTH> receive_buffer;
  std::size_t receive_index = 0;
  std::vector<std::uint8_t> send_buffer;
  std::size_t send_index = 0;
  bool receiving_finished = false;
};

/// Gopher server class.
class GopherServer {
public:
  GopherServer() = delete;                               // Default constructor.
  GopherServer(const GopherServer&) = delete;            // Copy constructor.
  GopherServer& operator=(const GopherServer&) = delete; // Copy assignment.
  GopherServer(GopherServer&&) noexcept;                 // Move constructor.
  GopherServer& operator=(GopherServer&&) noexcept;      // Move assignment.
  inline ~GopherServer() noexcept { destroy(); }         // Destructor.


private:
  void destroy() noexcept;

  std::expected<void, std::error_code> close_connection(tcp_utils::socket_t) noexcept;
  std::expected<void, std::error_code> add_connection(tcp_utils::socket_t) noexcept;
  std::expected<void, std::error_code> switchout_connection(tcp_utils::socket_t) noexcept;

  epoll_t epoll_fd = -1;                                                 // epoll file descriptor.
  tcp_utils::socket_t listen_fd = -1;                                    // Listen socket.
  std::unordered_map<tcp_utils::socket_t, ConnectionStatus> connections; // Connections map.
};

} // namespace server
