/// Main file of the project.

#include "./server/server.hpp"

#include <cstdlib>

#include <sys/epoll.h>
#include <unistd.h>

constexpr std::size_t MAX_EVENTS = 10;

using namespace server;

int main(int argc, char** argv) {
  auto server_result = GopherServer::build("0.0.0.0", 0);
  if (!server_result) {
    return EXIT_FAILURE;
  }
  const GopherServer server = std::move(server_result.value());

  return EXIT_SUCCESS;
}