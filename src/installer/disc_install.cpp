#include "installer/disc_install.h"

#include <rex/filesystem.h>
#include <rex/filesystem/devices/disc_image_device.h>
#include <rex/filesystem/devices/disc_image_entry.h>
#include <rex/filesystem/devices/stfs_container_device.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>
#include <rex/memory/mapped_memory.h>
#include <rex/string/utf8.h>
#include <rex/system/xcontent.h>

#include <array>
#include <format>
#include <fstream>
#include <utility>
#include <vector>

#include "core/logging.h"
#include "installer/dlc_publish.h"
#include "installer/extracted_disc.h"

namespace eot::installer {

namespace fs = std::filesystem;

std::unique_ptr<rex::filesystem::DiscImageDevice> OpenDiscImage(const fs::path &iso_path) {
  auto disc = std::make_unique<rex::filesystem::DiscImageDevice>("", iso_path);
  if (!disc->Initialize())
    return nullptr;
  return disc;
}

bool ValidateDisc(rex::filesystem::DiscImageDevice &disc) {
  return disc.ResolvePath("Default.xex") != nullptr && disc.ResolvePath("Data/GameLogic.dll") != nullptr;
}

std::string DiscFingerprint(const fs::path &iso_path, rex::filesystem::DiscImageDevice &disc) {
  std::error_code ec;
  const auto file_size = fs::file_size(iso_path, ec);
  size_t xex_size = 0;
  if (auto *entry = disc.ResolvePath("Default.xex"); entry != nullptr)
    xex_size = entry->size();
  return std::to_string(ec ? 0 : file_size) + ":" + std::to_string(xex_size);
}

namespace {

using namespace rex;
using namespace rex::system;
using namespace rex::filesystem;

}

PackageInfo InspectPackage(const fs::path &path) {
  PackageInfo info;
  auto header = StfsContainerDevice::ReadPackageHeader(path);
  if (!header) {
    info.error = "Not an Xbox 360 content package";
    return info;
  }
  info.ok = true;
  info.title_id = header->metadata.execution_info.title_id;
  info.content_type = static_cast<uint32_t>(static_cast<XContentType>(header->metadata.content_type));
  info.display_name = rex::string::to_utf8(header->metadata.display_name(XLanguage::kEnglish));
  return info;
}

std::string CheckUpdatePackage(const PackageInfo &info) {
  if (info.title_id != kTitleId)
    return std::format("Not this game's package (title {:08X})", info.title_id);
  if (info.content_type != kContentTypeInstaller)
    return std::format("Not a title update (content type {:08X})", info.content_type);
  return {};
}

std::string CheckDlcPackage(const PackageInfo &info) {
  if (info.title_id != kTitleId)
    return std::format("Not this game's package (title {:08X})", info.title_id);
  if (info.content_type != kContentTypeMarketplace)
    return std::format("Not downloadable content (content type {:08X})", info.content_type);
  return {};
}

namespace {

void CollectFiles(Entry *dir, const std::string &prefix, std::vector<std::pair<std::string, Entry *>> &out) {
  for (const auto &child : dir->children()) {
    const std::string child_path = prefix.empty() ? child->name() : prefix + "/" + child->name();
    if (child->attributes() & kFileAttributeDirectory)
      CollectFiles(child.get(), child_path, out);
    else
      out.emplace_back(child_path, child.get());
  }
}

bool Fail(InstallProgress &progress, const std::string &message) {
  EOT_ERROR("[install] {}", message);
  progress.SetError(message);
  progress.failed.store(true);
  return false;
}

bool ExtractDiscFile(Entry *entry, const fs::path &dest_path, InstallProgress &progress) {
  std::error_code ec;
  fs::create_directories(dest_path.parent_path(), ec);
  const size_t size = entry->size();
  auto mapped =
      static_cast<DiscImageEntry *>(entry)->OpenMapped(rex::memory::MappedMemory::Mode::kRead, 0, 0);
  if (!mapped)
    return Fail(progress, "Failed to read: " + entry->path());
  std::ofstream out(dest_path, std::ios::binary | std::ios::trunc);
  if (!out)
    return Fail(progress, "Failed to create: " + dest_path.string());
  out.write(reinterpret_cast<const char *>(mapped->data()), static_cast<std::streamsize>(size));
  if (!out)
    return Fail(progress, "Failed to write: " + dest_path.string());
  progress.bytes_done.fetch_add(size);
  return true;
}

bool ExtractPackageFile(Entry *entry, const fs::path &dest_path, InstallProgress &progress) {
  std::error_code ec;
  fs::create_directories(dest_path.parent_path(), ec);
  File *file = nullptr;
  if (entry->Open(FileAccess::kFileReadData, &file) != X_STATUS_SUCCESS || !file)
    return Fail(progress, "Failed to open in package: " + entry->path());
  std::ofstream out(dest_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    file->Destroy();
    return Fail(progress, "Failed to create: " + dest_path.string());
  }
  std::vector<uint8_t> buffer(1u << 20);
  size_t offset = 0;
  const size_t size = entry->size();
  bool ok = true;
  while (offset < size) {
    size_t read = 0;
    const size_t want = std::min(buffer.size(), size - offset);
    if (file->ReadSync(std::span<uint8_t>(buffer.data(), want), offset, &read) != X_STATUS_SUCCESS || read == 0) {
      ok = Fail(progress, "Failed to read from package: " + entry->path());
      break;
    }
    out.write(reinterpret_cast<const char *>(buffer.data()), static_cast<std::streamsize>(read));
    if (!out) {
      ok = Fail(progress, "Failed to write: " + dest_path.string());
      break;
    }
    offset += read;
    progress.bytes_done.fetch_add(read);
  }
  file->Destroy();
  return ok;
}

constexpr std::array<const char *, 2> kUpdateFiles = {"Default.xexp", "Data/GameLogic.dllp"};

}

std::thread Installer::RunAsync(const InstallSources &sources, const fs::path &game_data_dest, bool repair,
                                InstallProgress &progress) {
  return std::thread([sources, game_data_dest, repair, &progress]() {
    auto finish = [&]() { progress.complete.store(true); };

    std::unique_ptr<rex::filesystem::DiscImageDevice> disc;
    Entry *root = nullptr;
    std::vector<ExtractedDiscFile> folder_files;
    if (!sources.disc.empty()) {
      std::error_code source_ec;
      if (fs::is_directory(sources.disc, source_ec)) {
        const std::string error = CollectExtractedDiscFiles(sources.disc, game_data_dest, folder_files);
        if (!error.empty()) {
          Fail(progress, error);
          finish();
          return;
        }
      } else {
        disc = OpenDiscImage(sources.disc);
        if (!disc) {
          Fail(progress, "Failed to open the disc image.");
          finish();
          return;
        }
        root = disc->ResolvePath("");
        if (!root) {
          Fail(progress, "The disc image has no root directory.");
          finish();
          return;
        }
      }
    }

    struct PlanItem {
      std::string path;
      fs::path dest;
      Entry *entry;
      fs::path host_file;
      size_t size;
      bool needs_copy;
    };
    std::vector<PlanItem> plan;
    size_t total_bytes = 0;

    auto will_copy = [&](const fs::path &dest, size_t size) -> bool {
      if (!repair)
        return true;
      std::error_code ec;
      return !(fs::exists(dest, ec) && fs::file_size(dest, ec) == size);
    };
    auto add_to = [&](std::string path, fs::path dest, Entry *entry, fs::path host_file, size_t size) {
      const bool needs_copy = will_copy(dest, size);
      if (needs_copy)
        total_bytes += size;
      plan.push_back({std::move(path), std::move(dest), entry, std::move(host_file), size, needs_copy});
    };
    auto add = [&](std::string path, Entry *entry, fs::path host_file, size_t size) {
      const fs::path dest = game_data_dest / fs::path(path);
      add_to(std::move(path), dest, entry, std::move(host_file), size);
    };

    std::vector<std::pair<std::string, Entry *>> disc_files;
    if (root)
      CollectFiles(root, "", disc_files);
    for (auto &[rel, entry] : disc_files) {
      if (rel.rfind("$SystemUpdate/", 0) == 0 || rel.rfind("$SystemUpdate\\", 0) == 0)
        continue;
      add(rel, entry, {}, entry->size());
    }
    for (const ExtractedDiscFile& file : folder_files)
      add(file.relative_path.generic_string(), nullptr, file.host_path, file.size);

    std::unique_ptr<StfsContainerDevice> update;
    if (!sources.update.empty()) {
      update = std::make_unique<StfsContainerDevice>("", sources.update);
      if (!update->Initialize()) {
        Fail(progress, "Failed to open the title update package.");
        finish();
        return;
      }
      size_t found = 0;
      for (const char *name : kUpdateFiles) {
        if (Entry *entry = update->ResolvePath(name); entry != nullptr) {
          add(name, entry, {}, entry->size());
          ++found;
        }
      }
      if (found == 0) {
        Fail(progress, "The title update package holds neither Default.xexp nor Data/GameLogic.dllp.");
        finish();
        return;
      }
    }

    for (const fs::path &package : sources.dlc) {
      const PackageInfo info = InspectPackage(package);
      if (!info.ok) {
        Fail(progress, package.filename().string() + ": " + info.error);
        finish();
        return;
      }
      std::error_code ec;
      const size_t size = fs::file_size(package, ec);
      const fs::path dlc_dir = game_data_dest.parent_path() / kDlcFolderName;
      add_to(std::string(kDlcFolderName) + "/" + package.filename().string(), dlc_dir / package.filename(),
             nullptr, package, size);
    }

    // Title-update/DLC outputs are not necessarily in the extracted disc's
    // inventory. Check the complete plan before creating or replacing files.
    if (!folder_files.empty()) {
      for (const auto& item : plan) {
        if (!item.needs_copy)
          continue;
        const std::string error = ValidateExtractedDiscDestination(sources.disc, item.dest);
        if (!error.empty()) {
          Fail(progress, error);
          finish();
          return;
        }
      }
    }

    progress.files_total.store(plan.size());
    progress.bytes_total.store(total_bytes);
    EOT_INFO("[install] plan ({}): {} files, {} bytes to copy into {}", repair ? "repair" : "full",
             plan.size(), total_bytes, game_data_dest.string());

    std::error_code ec;
    fs::create_directories(game_data_dest, ec);

    for (const auto &item : plan) {
      if (progress.canceled.load() || progress.failed.load())
        break;
      if (!item.needs_copy) {
        progress.files_done.fetch_add(1);
        continue;
      }
      progress.SetCurrentFile(item.path);
      const fs::path &dest = item.dest;
      bool ok = false;
      if (item.entry == nullptr) {
        fs::create_directories(dest.parent_path(), ec);
        fs::copy_file(item.host_file, dest, fs::copy_options::overwrite_existing, ec);
        ok = !ec;
        if (!ok)
          Fail(progress, "Failed to copy " + item.host_file.string() + ": " + ec.message());
        else
          progress.bytes_done.fetch_add(item.size);
      } else if (update && item.path.size() > 1 && item.path.back() == 'p' &&
                 (item.path == kUpdateFiles[0] || item.path == kUpdateFiles[1])) {
        ok = ExtractPackageFile(item.entry, dest, progress);
      } else {
        ok = ExtractDiscFile(item.entry, dest, progress);
      }
      if (!ok)
        break;
      progress.files_done.fetch_add(1);
    }

    if (!progress.failed.load() && !progress.canceled.load())
      EOT_INFO("[install] {} complete: {} files", repair ? "repair" : "install", plan.size());
    finish();
  });
}

}
