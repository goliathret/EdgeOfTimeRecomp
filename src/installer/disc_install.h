/**
 * @file    installer/disc_install.h
 * @brief   The game's disc image, title update and content packages
 *          extracted into the install's game folder.
 *
 *          After reblue's installer/disc_install (BSD 3-Clause, Tom Clay).
 *          Edge of Time ships on one disc; its title update and DLC come as
 *          Xbox content packages (LIVE/PIRS containers), which the SDK's
 *          STFS device reads. The update's two files, Default.xexp and
 *          Data/GameLogic.dllp, are laid beside the disc's Default.xex and
 *          GameLogic.dll: the SDK loader applies a sibling patch when it
 *          loads a module, so the game boots with the update without the
 *          port patching anything itself. DLC packages are kept as they
 *          are in the install's dlc folder, and published into the profile
 *          at every boot (installer/dlc_publish.h).
 * @license BSD 3-Clause, see LICENSE
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace rex::filesystem {
class DiscImageDevice;
}

namespace eot::installer {

struct InstallProgress {
  std::atomic<size_t> files_done{0};
  std::atomic<size_t> files_total{0};
  std::atomic<size_t> bytes_done{0};
  std::atomic<size_t> bytes_total{0};
  std::atomic<bool> complete{false};
  std::atomic<bool> failed{false};
  std::atomic<bool> canceled{false};

  std::mutex file_mutex;
  std::string current_file;

  std::mutex error_mutex;
  std::string error_message;

  std::string GetCurrentFile() {
    std::lock_guard lock(file_mutex);
    return current_file;
  }
  void SetCurrentFile(const std::string &f) {
    std::lock_guard lock(file_mutex);
    current_file = f;
  }
  std::string GetError() {
    std::lock_guard lock(error_mutex);
    return error_message;
  }
  void SetError(const std::string &e) {
    std::lock_guard lock(error_mutex);
    error_message = e;
  }
};

constexpr uint32_t kTitleId = 0x415608B2;
constexpr uint32_t kContentTypeMarketplace = 0x00000002;
constexpr uint32_t kContentTypeInstaller = 0x000B0000;

std::unique_ptr<rex::filesystem::DiscImageDevice> OpenDiscImage(const std::filesystem::path &iso_path);

bool ValidateDisc(rex::filesystem::DiscImageDevice &disc);

std::string DiscFingerprint(const std::filesystem::path &iso_path, rex::filesystem::DiscImageDevice &disc);

struct PackageInfo {
  bool ok = false;
  std::string error;
  std::string display_name;
  uint32_t title_id = 0;
  uint32_t content_type = 0;
};
PackageInfo InspectPackage(const std::filesystem::path &path);
std::string CheckUpdatePackage(const PackageInfo &info);
std::string CheckDlcPackage(const PackageInfo &info);

struct InstallSources {
  // Either an Xbox 360 disc image or its already extracted game root.
  std::filesystem::path disc;
  std::filesystem::path update;
  std::vector<std::filesystem::path> dlc;
};

class Installer {
public:
  static std::thread RunAsync(const InstallSources &sources, const std::filesystem::path &game_data_dest,
                              bool repair, InstallProgress &progress);
};

}
