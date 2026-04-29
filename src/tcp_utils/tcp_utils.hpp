// Some TCP sockets utilities.

#include <cstdint>
#include <expected>
#include <string>
#include <vector>
#include <system_error>

#include <netinet/in.h>

namespace tcp_utils {

/// Options used by the listener socket (wrapper for setsockopt)
/// Not all the options are here, only the ones used.
/// The default values are equivalent to not setting them.
struct SocketOption {
  bool so_reuseaddr = false;
  std::uint32_t so_sndtimeo = 0;
  std::uint32_t so_rcvtimeo = 0;
  bool o_nonblock = false;
};

std::expected<void, std::error_code> set_socket_opts(int, SocketOption) noexcept;

std::expected<in_addr_t, std::error_code> to_net_addr(const std::string&) noexcept;
std::expected<std::string, std::error_code> to_str_addr(in_addr_t) noexcept;

std::expected<std::pair<std::uint32_t, std::string>, std::error_code> get_socket_local_addr(int) noexcept;
std::expected<std::pair<std::uint32_t, std::string>, std::error_code> get_socket_remote_addr(int) noexcept;

std::expected<sockaddr_in, std::error_code> make_sockaddr(std::uint32_t, const std::string&) noexcept;

std::expected<int, std::error_code> create_listen_socket(
    std::uint32_t, const std::string&, SocketOption, std::uint32_t) noexcept;


std::expected<void, std::error_code> send_t(int fd, const std::vector<uint8_t>&) noexcept;
std::expected<std::vector<uint8_t>, std::error_code> recv_t(int fd, std::size_t) noexcept;

} // namespace tcp_utils
