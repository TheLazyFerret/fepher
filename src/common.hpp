/// Common utilities used by the main.

#pragma once

#include <csignal>
#include <expected>
#include <string_view>
#include <string>
#include <system_error>

namespace common {

/// Declaration of the variable, will be defined in common.cpp.
extern volatile std::sig_atomic_t run;

/// Structure representing the parsed command line arguments.
struct args {};

std::string current_time() noexcept;

std::expected<void, std::error_code> set_signal_handler() noexcept;

void print_info(std::string_view str) noexcept;
void print_warning(std::string_view str) noexcept;
void print_error(std::string_view str) noexcept; 

} // namespace common
