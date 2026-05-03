/// Gopher protocol functions.
/// In this "module" is where most of the gopher protocol logic is.


/// Directory respose be like (see RFC 1436):
/// <File type>\r<name>\r<selector>\r<server address>\r<port>\r\n
/// {Any number of those}
/// . {And server close connection.}

/// File respose be like (see RFC 1436):
/// <Content of the file>
/// . {End with '.' in a separated line}

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

std::expected<std::string, std::error_code> create_directory_responde(const std::string& path) noexcept;

} // namespace gopher
