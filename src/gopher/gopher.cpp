/// gopher implementation.

/// Directory respose be like (see RFC 1436):
/// <File type><name>\t<selector>\t<server address>\t<port>\r\n
/// {Any number of those}
/// .\r\n {And server close connection.}

/// File respose be like (see RFC 1436):
/// <Content of the file>

#include "./gopher.hpp"

#include <cassert>
#include <expected>
#include <filesystem>
#include <string_view>
#include <system_error>

using namespace gopher;

namespace fs = std::filesystem;

/// Auxiliar function of get_gopher_type, return the gopher type depending of the file's extension.
std::string extension_gopher_type(std::string_view extension) noexcept {
  if (extension == "txt" || extension == "md") {
    return "0";
  } else if (extension == "png" || extension == "jpg" || extension == "jpeg") {
    return "I";
  } else if (extension == "gif") {
    return "g";
  }
  return "9"; // Default to binary file.
}

std::expected<std::string, std::error_code> gopher::get_gopher_type(const std::filesystem::path& path) noexcept {
  std::error_code err;
  const auto file_status = fs::status(path, err);
  if (err) {
    return std::unexpected(err);
  }
  if (fs::is_directory(file_status)) {
    return "1";
  } else if (fs::is_regular_file(file_status)) {
    if (path.has_extension()) {
      return extension_gopher_type(path.extension().string());
    } else {
      return "9"; // Default to binary.
    }
  } else {
    return std::unexpected(std::make_error_code(std::errc::file_exists));
  }
}

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
fs::path gopher::get_selector_without_terminator(std::string_view selector) noexcept {
  assert(selector.size() >= 2);
  assert(selector[selector.size() - 2] == TERMINATOR[0] && selector[selector.size() - 1] == TERMINATOR[1]);
  return std::string(selector.substr(0, selector.size() - 2));
}

/// Return the cannonical path of the parameter path.
std::expected<fs::path, std::error_code> gopher::get_real_path(const fs::path& path) noexcept {
  std::error_code er;
  const auto new_path = fs::canonical(path, er);
  if (er) {
    return std::unexpected(er);
  }
  return new_path;
}

/// Check if the path is inside of the base path.
/// It expects the parameter are absolute paths.
bool gopher::is_path_safe(const fs::path& base, const fs::path& path) noexcept {
  assert(base.is_absolute() && path.is_absolute());
  auto rel = path.lexically_relative(base);
  return !rel.empty() && *rel.begin() != "..";
}

std::expected<std::string, std::error_code> gopher::get_directory_responde(
    const fs::path& base, const fs::path& path, std::string_view port, std::string_view addr) noexcept {
  std::error_code er;
  const auto dir_iterator = fs::directory_iterator(path, er);
  if (er) {
    return std::unexpected(er);
  }
  std::string response; // The returned string with the complete response.
  for (const auto& dir_entry : dir_iterator) {
    // Directory iterator do not interate in ".." and ".".
    const auto& entry_path = dir_entry.path(); // The complete path of the file.
    const auto filename = entry_path.filename().string(); // The name of the file.
    const auto filetype_result = gopher::get_gopher_type(entry_path); // The gopher type.
    if (!filetype_result) {
      return std::unexpected(filetype_result.error());
    }
    // Append the new line to the response.
    response += filetype_result.value();
    response += filename;
    response += SEPARATOR;
    response += get_selector_response_path(base, entry_path);
    response += SEPARATOR;
    response += addr;
    response += SEPARATOR;
    response += port;
    response += TERMINATOR;
  }
  response += ".";
  response += TERMINATOR;
  return response;
}

/// Convert the system path into a gopher path.
std::string gopher::get_selector_response_path(const fs::path& base, const fs::path& path) noexcept {
  std::error_code er;
  auto result = fs::relative(path, base);
  if (er) {
    return "/";
  }  
  if (result == ".") {
    return "/";
  }
  return "/" + result.string();
}

fs::path gopher::get_path(const fs::path& base, const fs::path& sel) noexcept {
  if (sel == "." || sel.empty()) {
    return base;
  }
  return  base / sel.relative_path();
}