/// Source file of the common.hpp.

#include "./common.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <print>
#include <system_error>

/// Definition of the global variable from common.hpp.
volatile std::sig_atomic_t common::run = 1;

/// Signal handler.
extern "C" void handler(int signum) {
  switch (signum) {
  case SIGINT:
  case SIGTERM:
  case SIGQUIT:
    common::run = 0;
    break;
  default:
    break;
  }
}

/// Return a string with the date and current hour-minute.
std::string common::current_time() noexcept {
  const auto time = std::chrono::system_clock::now();
  return std::format("{:%Y/%m/%d %R}", time);
}

/// Set all the neccesary signal to a signal hander.
std::expected<void, std::error_code> common::set_signal_handler() noexcept {
  struct sigaction sa{};
  sa.sa_handler = handler;
  sa.sa_flags = 0;
  if (sigemptyset(&sa.sa_mask) < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  if (sigaction(SIGINT, &sa, nullptr) < 0) { // Stop from crtl+c
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  if (sigaction(SIGTERM, &sa, nullptr) < 0) { // Stop from kill()
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  if (sigaction(SIGQUIT, &sa, nullptr) < 0) { // Stop
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {};
}

/// Print in standard output a info message.
void common::print_info(std::string_view str) noexcept {
  std::println("{} INFO {}", common::current_time(), str);
}

/// Print in standard output a warning message.
void common::print_warning(std::string_view str) noexcept {
  std::println("{} WARNING {}", common::current_time(), str);
}

/// Print in standard output a error message.
void common::print_error(std::string_view str) noexcept {
  std::println("{} ERROR {}", common::current_time(), str);  
}
