#include "installer/extracted_disc.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace eot::installer {
namespace {

namespace fs = std::filesystem;

std::string PathText(const fs::path& path) {
  const auto text = path.generic_u8string();
  return std::string(text.begin(), text.end());
}

std::string PathError(const char* message, const fs::path& path, const std::error_code& ec) {
  return std::string(message) + ": " + PathText(path) + (ec ? " (" + ec.message() + ")" : "");
}

bool IsMissing(const std::error_code& ec) { return ec == std::errc::no_such_file_or_directory; }

// Check the unnormalized path first: normalizing link/../file could hide a
// redirected ancestor. The caller only canonicalizes after this succeeds.
std::string CheckPathChain(const fs::path& path) {
  bool leaf = true;
  for (fs::path current = path; !current.empty();) {
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(current, ec);
    if (ec && !IsMissing(ec)) return PathError("Could not inspect path", current, ec);
    if (fs::is_symlink(status)) return PathError("Symbolic links are not supported", current, {});
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(current.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
      const DWORD error = GetLastError();
      if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
        return PathError("Could not inspect path", current,
                         std::error_code(static_cast<int>(error), std::system_category()));
    } else if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      return PathError("Reparse points are not supported", current, {});
    }
#endif
    if (!leaf && fs::exists(status) && !fs::is_directory(status))
      return PathError("A path ancestor is not a directory", current, {});
    const fs::path parent = current.parent_path();
    if (parent == current) break;
    current = parent;
    leaf = false;
  }
  return {};
}

bool SameComponent(const fs::path& a, const fs::path& b) {
#if defined(_WIN32)
  const auto& left = a.native();
  const auto& right = b.native();
  return CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()), right.c_str(),
                              static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
#else
  return a == b;
#endif
}

bool ContainsPath(const fs::path& parent, const fs::path& child) {
  auto next = child.begin();
  for (const auto& part : parent) {
    if (next == child.end() || !SameComponent(part, *next)) return false;
    ++next;
  }
  return true;
}

std::string SourcePath(const fs::path& root, fs::path& source) {
  if (root.empty()) return "No extracted game folder was selected";
  std::error_code ec;
  source = fs::absolute(root, ec);
  if (ec) return PathError("Could not resolve extracted game folder", root, ec);
  if (const auto error = CheckPathChain(source); !error.empty()) return error;
  if (!fs::is_directory(source, ec)) return PathError("Not an extracted game folder", root, ec);
  source = fs::canonical(source, ec);
  if (ec) return PathError("Could not resolve extracted game folder", root, ec);
  return {};
}

std::string CheckRequiredFile(const fs::path& path) {
  if (const auto error = CheckPathChain(path); !error.empty()) return error;
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) return PathError("Missing required game file", path, ec);
  const auto size = fs::file_size(path, ec);
  if (ec) return PathError("Could not read game file size", path, ec);
  if (size == 0) return PathError("Required game file is empty", path, {});
  std::ifstream in(path, std::ios::binary);
  char first;
  if (!in.get(first)) return PathError("Could not read required game file", path, {});
  return {};
}

std::string CheckDestinationFile(const fs::path& path) {
  if (const auto error = CheckPathChain(path); !error.empty()) return error;
  std::error_code ec;
  const fs::file_status status = fs::symlink_status(path, ec);
  if (IsMissing(ec) || status.type() == fs::file_type::not_found) return {};
  if (ec) return PathError("Could not inspect destination file", path, ec);
  if (!fs::is_regular_file(status)) return PathError("Destination is not a regular file", path, {});
  // Truncating a hard-linked destination could also modify a source file under
  // a different name. Refuse it instead of writing through another file's link.
  const auto links = fs::hard_link_count(path, ec);
  if (ec) return PathError("Could not inspect destination file links", path, ec);
  if (links > 1) return PathError("Destination file has hard links", path, {});
  return {};
}

std::string Collect(const fs::path& root, const fs::path& destination,
                    std::vector<ExtractedDiscFile>& files) {
  fs::path source;
  if (const auto error = SourcePath(root, source); !error.empty()) return error;
  for (const fs::path& required : {fs::path("Default.xex"), fs::path("Data/GameLogic.dll")})
    if (const auto error = CheckRequiredFile(source / required); !error.empty()) return error;

  fs::path target;
  std::error_code ec;
  if (!destination.empty()) {
    target = fs::absolute(destination, ec);
    if (ec) return PathError("Could not resolve install destination", destination, ec);
    if (const auto error = CheckPathChain(target); !error.empty()) return error;
    const fs::file_status status = fs::symlink_status(target, ec);
    if (ec && !IsMissing(ec)) return PathError("Could not inspect install destination", target, ec);
    if (fs::exists(status) && !fs::is_directory(status))
      return PathError("Install destination is not a directory", target, {});
    ec.clear();
    target = fs::weakly_canonical(target, ec);
    if (ec) return PathError("Could not resolve install destination", destination, ec);
    if (ContainsPath(source, target) || ContainsPath(target, source))
      return "Extracted game folder and install destination must not overlap";
  }

  std::vector<ExtractedDiscFile> collected;
  fs::recursive_directory_iterator it(source, fs::directory_options::none, ec), end;
  if (ec) return PathError("Could not read extracted game folder", source, ec);
  while (it != end) {
    const fs::path host = it->path();
    if (const auto error = CheckPathChain(host); !error.empty()) return error;
    const fs::path relative = host.lexically_relative(source);
    if (relative.empty() || relative.is_absolute())
      return PathError("Invalid extracted game file path", host, {});
    for (const auto& part : relative)
      if (part == "..")
        return PathError("Extracted game file is outside its source folder", host, {});
    if (SameComponent(*relative.begin(), fs::path("$SystemUpdate"))) {
      it.disable_recursion_pending();
    } else {
      const fs::file_status status = it->symlink_status(ec);
      if (ec) return PathError("Could not inspect extracted game file", host, ec);
      if (!fs::is_directory(status)) {
        if (!fs::is_regular_file(status))
          return PathError("Extracted game entry is not a regular file", host, {});
        const auto size = it->file_size(ec);
        if (ec) return PathError("Could not read extracted game file size", host, ec);
        if (size > std::numeric_limits<size_t>::max())
          return PathError("Extracted game file is too large", host, {});
        std::ifstream in(host, std::ios::binary);
        if (!in) return PathError("Could not read extracted game file", host, {});
        if (!target.empty())
          if (const auto error = CheckDestinationFile(target / relative); !error.empty())
            return error;
        collected.push_back({relative, host, static_cast<size_t>(size)});
      }
    }
    it.increment(ec);
    if (ec) return PathError("Could not read extracted game folder", host.parent_path(), ec);
  }
  std::sort(collected.begin(), collected.end(), [](const auto& a, const auto& b) {
#if defined(_WIN32)
    const auto& left = a.relative_path.native();
    const auto& right = b.relative_path.native();
    const int order = CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()),
                                           right.c_str(), static_cast<int>(right.size()), TRUE);
    if (order != CSTR_EQUAL) return order == CSTR_LESS_THAN;
#endif
    return a.relative_path.native() < b.relative_path.native();
  });
#if defined(_WIN32)
  for (size_t i = 1; i < collected.size(); ++i)
    if (SameComponent(collected[i - 1].relative_path, collected[i].relative_path))
      return PathError("Extracted game files differ only by letter case",
                       collected[i].relative_path, {});
#endif
  files = std::move(collected);
  return {};
}

}  // namespace

std::string ValidateExtractedDisc(const std::filesystem::path& root) {
  try {
    fs::path source;
    if (const auto error = SourcePath(root, source); !error.empty()) return error;
    if (const auto error = CheckRequiredFile(source / "Default.xex"); !error.empty()) return error;
    return CheckRequiredFile(source / "Data/GameLogic.dll");
  } catch (const fs::filesystem_error& error) {
    return std::string("Could not inspect extracted game folder: ") + error.what();
  }
}

std::string CollectExtractedDiscFiles(const std::filesystem::path& source_root,
                                      const std::filesystem::path& game_data_dest,
                                      std::vector<ExtractedDiscFile>& files) {
  files.clear();
  try {
    return Collect(source_root, game_data_dest, files);
  } catch (const fs::filesystem_error& error) {
    return std::string("Could not inspect extracted game files: ") + error.what();
  }
}

std::string ValidateExtractedDiscDestination(const std::filesystem::path& root,
                                             const std::filesystem::path& destination_file) {
  try {
    fs::path source;
    if (const auto error = SourcePath(root, source); !error.empty()) return error;
    if (destination_file.empty()) return "No install destination file was selected";
    std::error_code ec;
    const fs::path destination = fs::absolute(destination_file, ec);
    if (ec) return PathError("Could not resolve destination file", destination_file, ec);
    if (const auto error = CheckDestinationFile(destination); !error.empty()) return error;
    const fs::path resolved = fs::weakly_canonical(destination, ec);
    if (ec) return PathError("Could not resolve destination file", destination_file, ec);
    if (ContainsPath(source, resolved))
      return "Install output must not overwrite the extracted game folder";
    return {};
  } catch (const fs::filesystem_error& error) {
    return std::string("Could not inspect install destination: ") + error.what();
  }
}

std::string ExtractedDiscFingerprint(const std::filesystem::path& root) {
  std::vector<ExtractedDiscFile> files;
  if (!CollectExtractedDiscFiles(root, {}, files).empty()) return {};
  size_t total = 0;
  size_t xex_size = 0;
  for (const auto& file : files) {
    if (file.size > std::numeric_limits<size_t>::max() - total) return {};
    total += file.size;
    if (SameComponent(file.relative_path, fs::path("Default.xex"))) xex_size = file.size;
  }
  return "folder:" + std::to_string(total) + ":" + std::to_string(xex_size);
}

}  // namespace eot::installer
