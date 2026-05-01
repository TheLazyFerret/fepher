/// Gopher server.

#pragma once

#include "../tcp_utils/tcp_utils.hpp"

#include <array>
#include <bits/types/timer_t.h>
#include <cstdint>
#include <expected>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

namespace server {

using epoll_t = int;
using timer_t = int;

constexpr std::size_t MAX_MESSAGE_LENGTH = 1024;
constexpr std::size_t BACKLOG = 10;
constexpr std::size_t MAX_EVENTS = 20;
constexpr int EPOLL_WAIT_TIME = 3000;          // 3 seconds.
constexpr std::size_t CONNECTION_TIMEOUT = 10; // in seconds.

enum class EpollMode { Epollin, Epollout };

/// Auxiliar struct of
/// Represents each connection and its state.
struct ConnectionStatus {
  std::array<std::uint8_t, MAX_MESSAGE_LENGTH> receive_buffer;
  std::size_t receive_index = 0;
  std::vector<std::uint8_t> send_buffer;
  std::size_t send_index = 0;
  bool receiving_finished = false;
  // It does not control the timer; it exists solely as a reference to find it on the timers map.
  timer_t timer = -1;
};

/// Gopher server class.
class GopherServer {
public:
  GopherServer(const GopherServer&) = delete;            // Copy constructor.
  GopherServer& operator=(const GopherServer&) = delete; // Copy assignment.
  GopherServer(GopherServer&&) noexcept;                 // Move constructor.
  GopherServer& operator=(GopherServer&&) noexcept;      // Move assignment.
  inline ~GopherServer() noexcept { destroy(); }         // Destructor.

  static std::expected<GopherServer, std::error_code> build(sockaddr_in) noexcept;

  std::expected<void, std::error_code> run() noexcept;

private:
  inline GopherServer() noexcept : epoll_fd(-1), listen_fd(-1) {} // Default constructor.

  /// -- Destructor helper.
  void destroy() noexcept;

  /// -- Build auxiliar method.
  static std::expected<epoll_t, std::error_code> create_epoll() noexcept;

  /// -- Epoll modifiers.
  std::expected<void, std::error_code> add_connection(tcp_utils::socket_t) noexcept;
  std::expected<void, std::error_code> switch_connection_mode(tcp_utils::socket_t, EpollMode) noexcept;
  void close_connection(tcp_utils::socket_t) noexcept;
  void close_timer(timer_t) noexcept;

  /// -- Execution auxiliar methods.
  void handle_incomming_connection() noexcept;
  void handle_connection(tcp_utils::socket_t) noexcept;
  static std::expected<void, std::error_code> connection_recv(tcp_utils::socket_t, ConnectionStatus&) noexcept;

  void close_listen_socket() noexcept;

  /// -- Attributes.
  epoll_t epoll_fd;                                                      // epoll file descriptor.
  tcp_utils::socket_t listen_fd;                                         // Listen socket.
  std::unordered_map<tcp_utils::socket_t, ConnectionStatus> connections; // Connections map.
  std::unordered_map<timer_t, tcp_utils::socket_t> timers;               // Timmers map.
};

} // namespace server
