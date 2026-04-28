/// Main file of the project.

#include "./tcp_utils/tcp_utils.hpp"
#include "./common/common.hpp"

#include <cstdlib>
#include <format>

#include <sys/epoll.h>
#include <unistd.h>

constexpr std::size_t MAX_EVENTS = 10;

int main(int argc, char** argv) {
  const auto fd_result = tcp_utils::create_listen_socket(0, "0.0.0.0", {}, 10);
  if (!fd_result) {
    common::print_error(std::format("{}", fd_result.error().message()));
    return EXIT_FAILURE;
  }
  const int listen_fd = fd_result.value();
  // Should not fail, due the socket is valid.
  const auto addr_result = tcp_utils::get_socket_addr(listen_fd).value();
  common::print_info(std::format("listen socket: {} in port: {}", listen_fd, addr_result.first));
  
  
  const int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    common::print_error("Failed to create epollfd");
    return EXIT_FAILURE;
  }
  common::print_info(std::format("epoll file descriptor: {}", epoll_fd));
  
  epoll_event listen_ev;
  listen_ev.events = EPOLLIN | EPOLLET;
  listen_ev.data.fd = listen_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &listen_ev) < 0) {
    common::print_error("Failed to add listen to the epoll events");
  }
  
  epoll_event events[MAX_EVENTS] {};
  while(true) {
    const int number_of_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if (number_of_events < 0) {
      common::print_error("Failed to wait");
      return EXIT_FAILURE;
    }
    common::print_info("Connected!");
  }

  close(epoll_fd);
  close(listen_fd);
  return EXIT_SUCCESS;
}