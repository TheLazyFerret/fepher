/// RAID Abstraction from posix sockets.

#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <system_error>
#include <vector>

/// Options used by the listener socket (wrapper for setsockopt)
/// Not all the options are here, only the ones used.
/// The default values are equivalent to not setting them.
struct SocketOption {
  bool so_reuseaddr = false;
  std::uint32_t so_sndtimeo = 0;
  std::uint32_t so_rcvtimeo = 0;
};

class TcpConnection;

/// Listener TCP socket.
class TcpListener {
public:
  /// Special methods
  TcpListener() = delete;                              // Default constructor.
  inline ~TcpListener() noexcept { destroy(); }        // Destructor.
  TcpListener(const TcpListener&) = delete;            // Copy constructor.
  TcpListener& operator=(const TcpListener&) = delete; // Copy assignment.
  TcpListener(TcpListener&&) noexcept;                 // Move constructor.
  TcpListener& operator=(TcpListener&&) noexcept;      // Move assignment.

  std::expected<TcpConnection, std::error_code> accept_connection() const noexcept;

  static std::expected<TcpListener, std::error_code> build(
      std::uint16_t, const std::string&, SocketOption, std::uint16_t) noexcept;

private:
  void destroy() noexcept;
  explicit inline TcpListener(int fs) noexcept : socket_fd(fs) {} // Constructor.

  int socket_fd; // Internal socket file descriptor.
};

/// Connection TCP socket.
class TcpConnection {
public:
  TcpConnection() = delete;                                // Default constructor.
  inline ~TcpConnection() noexcept { destroy(); }          // Destructor.
  TcpConnection(const TcpConnection&) = delete;            // Copy constructor.
  TcpConnection& operator=(const TcpConnection&) = delete; // Copy assignment.
  TcpConnection(TcpConnection&&) noexcept;                 // Move constructor.
  TcpConnection& operator=(TcpConnection&&) noexcept;      // Move assignment.

  std::pair<std::string, std::uint16_t> get_address() const noexcept;

  std::expected<void, std::error_code> send_t(const std::vector<uint8_t>&) const noexcept;
  std::expected<std::vector<uint8_t>, std::error_code> recv_t(std::size_t len) const noexcept;

private:
  friend TcpListener;

  explicit inline TcpConnection(int fs, const std::string addr, std::uint16_t port) noexcept
      : socket_fd(fs), str_addr(addr), port(port) {}
  void destroy() noexcept;

  int socket_fd;
  std::string str_addr;
  std::uint16_t port;
};
