/// Gopher protocol functions.
/// In this "module" is where most of the gopher protocol logic is.

#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <system_error>

namespace gopher {

constexpr std::string_view TERMINATOR = "\r\n";

bool string_end_in_terminator(std::string_view) noexcept;
std::string get_selector_without_terminator(std::string_view) noexcept;
std::string get_path(const std::string& path, const std::string& fileserver) noexcept;
std::expected<std::string, std::error_code> get_real_path(const std::string&) noexcept;
bool is_path_safe(std::string_view path, std::string_view fileserver) noexcept;

} // namespace gopher
