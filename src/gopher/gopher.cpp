/// Gopher protocol auxiliar methods implementation.

#include "./gopher.hpp"
#include <string_view>

constexpr std::string_view TERMINATOR = "\r\n";

/// Check if the string end in the terminator.
bool gopher::string_end_in_terminator(std::string_view e) noexcept {
  if (e.size() < 2) {
    return false;
  }

  if (e[e.size() - 2] == TERMINATOR.at(0) && e[e.size() - 1] == TERMINATOR.at(1)) {
    return true;
  }
  return false;
}
