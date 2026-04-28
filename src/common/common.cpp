/// Source file of the common.hpp.

#include "./common.hpp"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <expected>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <grp.h> // For working with /etc/group database.
#include <pwd.h> // For working with /etc/passwd database.
#include <sys/types.h>
#include <unistd.h>

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
void common::print_info(std::string_view str) noexcept { std::println("{} INFO {}", common::current_time(), str); }

/// Print in standard output a warning message.
void common::print_warning(std::string_view str) noexcept { std::println("{} WARN {}", common::current_time(), str); }

/// Print in standard output a error message.
void common::print_error(std::string_view str) noexcept { std::println("{} ERROR {}", common::current_time(), str); }

/// Transform the c style argument array into a c++ vector.
std::vector<std::string> transform_args(int argc, char** argv) noexcept {
  assert(argc > 0);
  std::vector<std::string> result;
  result.reserve(argc - 1);
  for (int i = 1; i < argc; ++i) {
    result.emplace_back(argv[i]);
  }
  return result;
}

/// Parse the command line arguments.
std::expected<common::args, std::string> parse_arguments(int argc, char** argv) {
  const auto args = transform_args(argc, argv);
  return {};
}

/// Return true if the user id of the process is root.
bool common::uid_is_root() noexcept {
  if (geteuid() == 0) {
    return true;
  }
  return false;
}

/// Return (if exist) the uid of a user from /etc/passwd.
std::optional<uid_t> common::get_uid(const std::string& user) noexcept {
  const auto aux = getpwnam(user.c_str());
  if (aux == nullptr) {
    return std::nullopt;
  }
  return aux->pw_uid;
}

/// Return (if exist) the gid of a user from /etc/passwd.
std::optional<gid_t> common::get_gid(const std::string& group) noexcept {
  const auto aux = getgrnam(group.c_str());
  if (aux == nullptr) {
    return std::nullopt;
  }
  return aux->gr_gid;
}

/// Attempts to change the uid and the gid of the process.
std::expected<void, std::error_code> common::change_user_group_id(
    const std::string& user, const std::string& group) noexcept {
  // Check if the current efective user is root.
  if (!common::uid_is_root()) {
    return std::unexpected(std::make_error_code(std::errc::permission_denied));
  }
  // Retrieve the uid.
  const auto uid = common::get_uid(user);
  if (!uid.has_value()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  // Retrieve the gid.
  const auto gid = common::get_gid(group);
  if (!gid.has_value()) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  // Initialize the suplementary group access list to the new uid.
  if (initgroups(user.c_str(), gid.value()) < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  // Change the gid.
  if (setgid(gid.value()) < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  // Change the uid.
  if (setuid(uid.value()) < 0) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {};
}
