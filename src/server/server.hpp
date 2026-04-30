/// Gopher server.

#pragma once

#include "../tcp_utils/tcp_utils.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <sys/socket.h>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace server {

using epoll_t = int;

constexpr std::size_t MAX_MESSAGE_LENGTH = 1024;
constexpr std::size_t BAKCLOG = 10;
constexpr std::size_t MAX_EVENTS = 20;
constexpr int EPOLL_WAIT_TIME = 3000; // 3 seconds.

enum class EpollMode { Epollin, Epollout };

/// Auxiliar struct of
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

  /// -- Execution auxiliar methods.
  void handle_incomming_connection() noexcept;
  void handle_connection(tcp_utils::socket_t) noexcept;
  static std::expected<void, std::error_code> connection_recv(tcp_utils::socket_t, ConnectionStatus&) noexcept;

  void close_listen_socket() noexcept;

  /// -- Attributes.
  epoll_t epoll_fd;                                                      // epoll file descriptor.
  tcp_utils::socket_t listen_fd;                                         // Listen socket.
  std::unordered_map<tcp_utils::socket_t, ConnectionStatus> connections; // Connections map.
};

} // namespace server
