/// Main file of the project.

#include "./server/server.hpp"
#include "./tcp_utils/tcp_utils.hpp"
#include "common/common.hpp"

#include <cstdlib>
// #include <format>

#include <sys/epoll.h>
#include <unistd.h>

using namespace server;

int main() {
  const auto addr = tcp_utils::make_sockaddr(0, "0.0.0.0").value();
  auto server_result = GopherServer::build(std::move(addr));
  if (!server_result) {
    common::print_error("Failed to create server");
    return EXIT_FAILURE;
  }
  GopherServer sv = std::move(server_result.value());
  if (!sv.run()) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
