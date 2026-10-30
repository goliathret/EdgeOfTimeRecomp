#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace eot::installer {

struct ExtractedDiscFile {
  std::filesystem::path relative_path;
  std::filesystem::path host_path;
  size_t size;
};

// An empty destination requests a read-only inventory, without destination checks.
// On failure, files is empty and the returned string explains the problem.
std::string CollectExtractedDiscFiles(const std::filesystem::path& source_root,
                                      const std::filesystem::path& game_data_dest,
                                      std::vector<ExtractedDiscFile>& files);

// Checks the same two required modules as the disc-image installer. This does
// not verify a retail hash manifest; title updates remain separate inputs.
std::string ValidateExtractedDisc(const std::filesystem::path& root);

// Checks a planned output, including title-update and DLC files, before writing.
std::string ValidateExtractedDiscDestination(const std::filesystem::path& root,
                                             const std::filesystem::path& destination_file);

// A source identifier, not a content hash: folder:<total bytes>:<Default.xex bytes>.
// Returns an empty string if the source cannot be inventoried.
std::string ExtractedDiscFingerprint(const std::filesystem::path& root);

}  // namespace eot::installer
