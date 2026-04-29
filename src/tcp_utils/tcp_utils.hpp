// Some TCP sockets utilities.

#include <cstdint>
#include <expected>
#include <string>
#include <system_error>
#include <vector>

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

/// Wrapper structure representing the address and the port of the socket in
/// his human-redeable form.
struct SocketAddress {
  std::string address;
  std::uint16_t port;
};

/// Small redefine for a more clear interface.
using socket_t = int;

/// Set the chosen options to a socket that is not already binded.
/// This function should not be called directly, due it already is in create_listen_socket().
std::expected<void, std::error_code> set_socket_opts(socket_t socket, SocketOptions ops) noexcept;
/// Return a ready-to-use sockaddr_in, with the correctly formated port and IPv4 address.
/// Again, this function should not be called directly, due it already is in create_listen_socket().
std::expected<sockaddr_in, std::error_code> make_sockaddr(std::uint16_t port, const std::string& addr) noexcept;

/// From a human readeable IPv4 address return a net form address.
std::expected<in_addr_t, std::error_code> to_net_addr(const std::string& addr) noexcept;
/// From a net IPv4 form address return a human redeable address.
std::expected<std::string, std::error_code> to_str_addr(in_addr_t addr) noexcept;

/// Return the local port and address a socket is connected to.
std::expected<SocketAddress, std::error_code> get_socket_local_addr(socket_t socket) noexcept;
/// Return the remote port and address a socket is connected to.
std::expected<SocketAddress, std::error_code> get_socket_remote_addr(socket_t socket) noexcept;

/// Create a listen, ready to connect TCP socket.
std::expected<socket_t, std::error_code> create_listen_socket(
    std::uint16_t port, const std::string& addr, SocketOptions ops, std::uint32_t backlog) noexcept;

/// Attempts to send all the bytes in the vector through the connected socket.
/// The function assumes the socket is set in blocking mode.
std::expected<void, std::error_code> blocking_send(socket_t socket, const std::vector<uint8_t>& buffer) noexcept;
/// Attempts to receive at most len bytes. The data retrieved will be returned in the form
/// of a vector, being its size the ammount of bytes retrieved.
/// The function assumes the socket is set in blocking mode.
std::expected<std::vector<uint8_t>, std::error_code> blocking_recv(socket_t socket, std::size_t len) noexcept;

} // namespace tcp_utils
