/// Gopher protocol  methods implementation.

#include "./gopher.hpp"

#include <cassert>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>

#include <limits.h>
#include <stdlib.h>

using namespace gopher;

/// Check if the string end in the terminator.
bool gopher::string_end_in_terminator(std::string_view selector) noexcept {
  if (selector.size() < 2) {
    return false;
  } else if (selector[selector.size() - 2] == TERMINATOR[0] && selector[selector.size() - 1] == TERMINATOR[1]) {
    return true;
  } else {
    return false;
  }
}

/// Return the received selector without the terminator (\r\n).
std::string gopher::get_selector_without_terminator(std::string_view selector) noexcept {
  assert(selector.size() >= 2);
  assert(selector[selector.size() - 2] == TERMINATOR[0] && selector[selector.size() - 1] == TERMINATOR[1]);
  return std::string(selector.substr(0, selector.size() - 2));
}

/// Get the path from the selector. This function assumes the path parameter doesn't end with "\r\n".
/// Basically just append '/' where is needed.
std::string gopher::get_path(const std::string& path, const std::string& fileserver) noexcept {
  assert(!gopher::string_end_in_terminator(path));
  assert(fileserver.size() > 0 && fileserver.back() == '/'); // The fileserver path should end with '/'.
  if (path.size() == 0) {
    return fileserver;
  } else if (path[0] == '/') {
    return fileserver + path.substr(1); // Ignores the path '/'.
  } else {
    return fileserver + path;
  }
}

/// Return the real path, also checking if the path is correct.
/// Basically a safer wrapper around realpath().
std::expected<std::string, std::error_code> gopher::get_real_path(const std::string& path) noexcept {
  char result[PATH_MAX]{};
  if (realpath(path.c_str(), result) == nullptr) {
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return {result};
}

/// Returns true if the path (assumes it is a complete and valid path, see get_real_path()) is inside the fileserver.
/// The path must not have any type of symbolic/hard links.
/// Also assumes fileserver_path is ended in '/'
bool gopher::is_path_safe(std::string_view path, std::string_view fileserver_path) noexcept {
  assert(fileserver_path.back() == '/');
  if (fileserver_path.size() > path.size()) {
    return false;
  }
  for (std::size_t n = 0; n < fileserver_path.size(); ++n) {
    if (path[n] != fileserver_path[n]) {
      return false;
    }
  }
  return true;
}
