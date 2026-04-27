/// Common utilities used by the main.

#pragma once

#include <csignal>
#include <expected>
#include <string_view>
#include <string>
#include <system_error>
#include <optional>
#include <vector>

#include <unistd.h>


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

std::vector<std::string> transform_args(int argc, char** argv) noexcept;
std::expected<args, std::error_code> parse_arguments(int argc, char** argv) noexcept;

bool uid_is_root() noexcept;

std::expected<void, std::error_code> change_user_group_id(const std::string& user, const std::string& group) noexcept;

std::optional<uid_t> get_uid(const std::string&) noexcept;
std::optional<gid_t> get_gid(const std::string&) noexcept;


} // namespace common
