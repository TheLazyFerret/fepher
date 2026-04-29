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
struct SocketOptions {
  bool so_reuseaddr = false;
  std::uint32_t so_sndtimeo = 0;
  std::uint32_t so_rcvtimeo = 0;
  bool o_nonblock = false;
};

/// Set the chosen options to a socket that is not already binded.
/// This function should not be called directly, due it already is in create_listen_socket().
std::expected<void, std::error_code> set_socket_opts(int, SocketOptions) noexcept;
/// Return a ready-to-use sockaddr_in, with the correctly formated port and IPv4 address.
/// Again, this function should not be called directly, due it already is in create_listen_socket().
std::expected<sockaddr_in, std::error_code> make_sockaddr(std::uint32_t, const std::string&) noexcept;

/// From a human readeable IPv4 address return a net form address.
std::expected<in_addr_t, std::error_code> to_net_addr(const std::string&) noexcept;
/// From a net IPv4 form address return a human redeable address.
std::expected<std::string, std::error_code> to_str_addr(in_addr_t) noexcept;

/// Return the local port and address a socket is connected to.
std::expected<std::pair<std::uint32_t, std::string>, std::error_code> get_socket_local_addr(int) noexcept;
/// Return the remote port and address a socket is connected to. 
std::expected<std::pair<std::uint32_t, std::string>, std::error_code> get_socket_remote_addr(int) noexcept;

/// Create a listen, ready to connect TCP socket.
/// Parameters: (port, IPv4 address, options, max connection queue).
std::expected<int, std::error_code> create_listen_socket(
    std::uint32_t, const std::string&, SocketOptions, std::uint32_t) noexcept;

/// Attempts to send all the bytes in the vector through the connected socket.
std::expected<void, std::error_code> send_t(int, const std::vector<uint8_t>&) noexcept;
/// Attempts to receive at most len bytes. The data retrieved will be returned in the form
/// of a vector, being its size the ammount of bytes retrieved.
std::expected<std::vector<uint8_t>, std::error_code> recv_t(int, std::size_t) noexcept;

} // namespace tcp_utils
