// Some TCP sockets utilities.

#pragma once

#include <cstdint>
#include <expected>
#include <string>
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

/// Attempts to accept an incoming connection.
std::expected<socket_t, std::error_code> accept_connection(socket_t) noexcept;

/// -- All this functions are for checking values.

bool try_again_later(int) noexcept;

bool connection_closed(int) noexcept;

} // namespace tcp_utils
