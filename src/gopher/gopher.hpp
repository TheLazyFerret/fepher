/// Gopher logic using modern c++ features.

#pragma once

#include <expected>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace gopher {

static constexpr auto TERMINATOR = "\r\n";
static constexpr auto SEPARATOR = "\t";

std::expected<std::string, std::error_code> get_gopher_type(const std::filesystem::path&) noexcept;

bool string_end_in_terminator(std::string_view) noexcept;
std::filesystem::path get_selector_without_terminator(std::string_view) noexcept;

std::expected<std::filesystem::path, std::error_code> get_real_path(const std::filesystem::path&) noexcept;
bool is_path_safe(const std::filesystem::path&, const std::filesystem::path&) noexcept;

std::string get_selector_response_path(const std::filesystem::path&, const std::filesystem::path&) noexcept;
std::expected<std::string, std::error_code> get_directory_responde(
    const std::filesystem::path&, const std::filesystem::path&, std::string_view, std::string_view) noexcept;

std::filesystem::path get_path(const std::filesystem::path&, const std::filesystem::path&) noexcept;

} // namespace gopher
