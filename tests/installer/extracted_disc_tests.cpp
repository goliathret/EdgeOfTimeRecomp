#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "installer/extracted_disc.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#endif

namespace {
namespace fs = std::filesystem;
using eot::installer::CollectExtractedDiscFiles;
using eot::installer::ExtractedDiscFile;
using eot::installer::ExtractedDiscFingerprint;
using eot::installer::ValidateExtractedDisc;
using eot::installer::ValidateExtractedDiscDestination;

void Require(bool value, const std::string& message) {
  if (!value) throw std::runtime_error(message);
}

void Write(const fs::path& path, const std::string& bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  Require(static_cast<bool>(out), "Could not write test fixture");
}

std::string Read(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  Require(static_cast<bool>(in), "Could not read test fixture");
  return std::string(std::istreambuf_iterator<char>(in), {});
}

struct Fixture {
  fs::path temp_parent = fs::canonical(fs::temp_directory_path());
  fs::path root;
  fs::path source;
  fs::path destination;

  Fixture() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
      root = temp_parent / ("reeot-extracted-disc-tests-" + std::to_string(stamp) + "-" +
                            std::to_string(attempt));
      std::error_code ec;
      if (fs::create_directory(root, ec)) break;
      if (ec) throw std::runtime_error("Could not create temporary fixture: " + ec.message());
      root.clear();
    }
    Require(!root.empty(), "Could not allocate a unique temporary fixture");
    source = root / fs::path(u8"disc with spaces \u0414\u043e\u043d\u043e\u0440");
    destination = root / "output with spaces";
    Write(source / "Default.xex", "test executable bytes");
    Write(source / "Data/GameLogic.dll", "test module bytes");
    Write(source / "Data/subfolder/asset.pkz", "test nested asset");
    Write(source / "video/intro.usm", "test video bytes");
  }

  ~Fixture() {
    // Only this constructor's unique, direct child of the temporary directory
    // is removed. remove_all removes link entries without following targets.
    const fs::path resolved = fs::absolute(root).lexically_normal();
    if (resolved.parent_path() == temp_parent &&
        resolved.filename().string().starts_with("reeot-extracted-disc-tests-")) {
      std::error_code ec;
      fs::remove_all(resolved, ec);
      if (ec) std::cerr << "Fixture cleanup failed: " << ec.message() << '\n';
    }
  }
};

std::vector<ExtractedDiscFile> Inventory(const fs::path& source, const fs::path& destination = {}) {
  std::vector<ExtractedDiscFile> files;
  const auto error = CollectExtractedDiscFiles(source, destination, files);
  Require(error.empty(), error);
  return files;
}

bool Has(const std::vector<ExtractedDiscFile>& files, const std::string& relative) {
  return std::any_of(files.begin(), files.end(), [&](const auto& file) {
    return file.relative_path.generic_string() == relative;
  });
}

void RejectPlan(const fs::path& source, const fs::path& destination) {
  std::vector<ExtractedDiscFile> files{{"stale", "stale", 1}};
  Require(!CollectExtractedDiscFiles(source, destination, files).empty(),
          "Unsafe or invalid plan unexpectedly succeeded");
  Require(files.empty(), "A failed plan exposed stale or partial files");
}

struct Skipped : std::runtime_error {
  using std::runtime_error::runtime_error;
};

void MakeSymlink(const fs::path& target, const fs::path& link, bool directory) {
  std::error_code ec;
  if (directory)
    fs::create_directory_symlink(target, link, ec);
  else
    fs::create_symlink(target, link, ec);
  if (ec) throw Skipped("Platform denied symbolic link creation: " + ec.message());
}

#if defined(_WIN32)
// An NTFS mount-point junction does not need the symbolic-link privilege.
void MakeJunction(const fs::path& target, const fs::path& link) {
  fs::create_directory(link);
  const std::wstring print = fs::absolute(target).native();
  const std::wstring substitute = L"\\??\\" + print;
  struct JunctionBuffer {
    DWORD tag;
    WORD data_length;
    WORD reserved;
    WORD substitute_offset;
    WORD substitute_length;
    WORD print_offset;
    WORD print_length;
    WCHAR path[8192];
  } buffer{};
  const size_t substitute_bytes = substitute.size() * sizeof(WCHAR);
  const size_t print_bytes = print.size() * sizeof(WCHAR);
  Require(substitute_bytes + print_bytes + 2 * sizeof(WCHAR) <= sizeof(buffer.path),
          "Junction fixture path too long");
  buffer.tag = IO_REPARSE_TAG_MOUNT_POINT;
  buffer.substitute_length = static_cast<WORD>(substitute_bytes);
  buffer.print_offset = static_cast<WORD>(substitute_bytes + sizeof(WCHAR));
  buffer.print_length = static_cast<WORD>(print_bytes);
  std::copy(substitute.begin(), substitute.end(), buffer.path);
  std::copy(print.begin(), print.end(), buffer.path + substitute.size() + 1);
  buffer.data_length = static_cast<WORD>(8 + substitute_bytes + print_bytes + 2 * sizeof(WCHAR));
  const HANDLE handle =
      CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                  FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    throw Skipped("Platform denied junction creation: " + std::to_string(GetLastError()));
  DWORD returned = 0;
  const BOOL ok = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, &buffer, buffer.data_length + 8,
                                  nullptr, 0, &returned, nullptr);
  const DWORD error = GetLastError();
  CloseHandle(handle);
  if (!ok) throw Skipped("Platform denied junction creation: " + std::to_string(error));
}
#endif

int RunTests() {
  int passed = 0, failed = 0, skipped = 0;
  const auto test = [&](const char* name, const auto& body) {
    try {
      body();
      ++passed;
      std::cout << "PASS " << name << '\n';
    } catch (const Skipped& error) {
      ++skipped;
      std::cout << "SKIP " << name << ": " << error.what() << '\n';
    } catch (const std::exception& error) {
      ++failed;
      std::cerr << "FAIL " << name << ": " << error.what() << '\n';
    }
  };

  test("readable extracted root with spaces and Unicode", [] {
    Fixture f;
    Require(ValidateExtractedDisc(f.source).empty(), "Valid extracted root rejected");
    const auto before = Read(f.source / "Default.xex");
    const auto files = Inventory(f.source, f.destination);
    Require(files.size() == 4 && Has(files, "Data/subfolder/asset.pkz") &&
                Has(files, "video/intro.usm"),
            "Incomplete inventory");
    for (const auto& file : files) {
      Require(file.host_path == f.source / file.relative_path,
              "Host path does not resolve the source");
      Require(file.size == fs::file_size(file.host_path), "Reported size differs from source");
    }
    Require(Read(f.source / "Default.xex") == before, "Inventory changed source bytes");
    Require(!fs::exists(f.destination), "Inventory created an install destination");
  });
  test("empty selection, missing root, and file instead of root", [] {
    Fixture f;
    Require(!ValidateExtractedDisc({}).empty(), "Empty selection accepted");
    RejectPlan({}, f.destination);
    RejectPlan(f.root / "missing", f.destination);
    RejectPlan(f.source / "Default.xex", f.destination);
    Require(ExtractedDiscFingerprint(f.root / "missing").empty(), "Invalid source fingerprinted");
  });
  for (const char* required : {"Default.xex", "Data/GameLogic.dll"}) {
    test((std::string("missing required module ") + required).c_str(), [=] {
      Fixture f;
      fs::remove(f.source / required);
      Require(!ValidateExtractedDisc(f.source).empty(), "Missing required module accepted");
      RejectPlan(f.source, f.destination);
    });
    test((std::string("empty required module ") + required).c_str(), [=] {
      Fixture f;
      Write(f.source / required, "");
      Require(!ValidateExtractedDisc(f.source).empty(), "Empty required module accepted");
      RejectPlan(f.source, f.destination);
    });
  }
  test("required module is a directory", [] {
    Fixture f;
    fs::remove(f.source / "Data/GameLogic.dll");
    fs::create_directory(f.source / "Data/GameLogic.dll");
    RejectPlan(f.source, f.destination);
  });
  test("top-level system update skipped, nested name retained", [] {
    Fixture f;
    Write(f.source / "$SystemUpdate/update.bin", "do not install");
    Write(f.source / "Data/$SystemUpdate/ordinary.pkz", "install nested asset");
    const auto files = Inventory(f.source);
    Require(!Has(files, "$SystemUpdate/update.bin"), "Console system update entered plan");
    Require(Has(files, "Data/$SystemUpdate/ordinary.pkz"), "Nested game asset was dropped");
    Require(fs::exists(f.source / "$SystemUpdate/update.bin"), "Skipped source was deleted");
  });
  test("stable inventory and size fingerprint", [] {
    Fixture f;
    Write(f.source / "Data/Z.pkz", "last");
    Write(f.source / "Data/A.pkz", "first");
    const auto a = Inventory(f.source), b = Inventory(f.source);
    Require(a.size() == b.size(), "Plan size changed between inventories");
    size_t total = 0;
    for (size_t i = 0; i < a.size(); ++i) {
      Require(a[i].relative_path == b[i].relative_path && a[i].host_path == b[i].host_path &&
                  a[i].size == b[i].size,
              "Repeated inventory changed");
      total += a[i].size;
    }
    const auto expected = "folder:" + std::to_string(total) + ":" +
                          std::to_string(fs::file_size(f.source / "Default.xex"));
    Require(ExtractedDiscFingerprint(f.source) == expected, "Unexpected size fingerprint");
    Write(f.source / "$SystemUpdate/update.bin", "excluded bytes");
    Require(ExtractedDiscFingerprint(f.source) == expected, "Excluded update changed fingerprint");
  });
  test("source/destination overlap rejected in both directions", [] {
    Fixture f;
    RejectPlan(f.source, f.source);
    RejectPlan(f.source, f.source / "install");
    RejectPlan(f.source, f.root);
    RejectPlan(f.source, f.source / "Data/..");
    fs::path separate = f.source;
    separate += "-output";
    Inventory(f.source, separate);
  });
  test("existing ordinary destination allows repair inventory", [] {
    Fixture f;
    Write(f.destination / "Default.xex", "old destination module");
    Inventory(f.source, f.destination);
    Require(Read(f.destination / "Default.xex") == "old destination module",
            "Plan wrote destination");
    RejectPlan(f.source, f.destination / "Default.xex");
  });
  test("destination hard link to donor rejected", [] {
    Fixture f;
    fs::create_directories(f.destination);
    fs::create_hard_link(f.source / "Default.xex", f.destination / "Default.xex");
    RejectPlan(f.source, f.destination);
    Require(Read(f.source / "Default.xex") == "test executable bytes",
            "Donor changed through hard link");
  });
  test("source directory symbolic link rejected", [] {
    Fixture f;
    MakeSymlink(f.source, f.root / "source link", true);
    RejectPlan(f.root / "source link", f.destination);
  });
  test("nested source file symbolic link rejected", [] {
    Fixture f;
    Write(f.root / "outside.bin", "outside bytes");
    MakeSymlink(f.root / "outside.bin", f.source / "Data/linked.pkz", false);
    RejectPlan(f.source, f.destination);
  });
  test("destination symbolic link ancestor rejected", [] {
    Fixture f;
    fs::create_directories(f.root / "other output");
    MakeSymlink(f.root / "other output", f.root / "output link", true);
    RejectPlan(f.source, f.root / "output link/new game");
  });
  test("existing destination file symbolic link rejected", [] {
    Fixture f;
    fs::create_directories(f.destination);
    MakeSymlink(f.source / "Default.xex", f.destination / "Default.xex", false);
    RejectPlan(f.source, f.destination);
  });
  test("additional update and DLC outputs are checked", [] {
    Fixture f;
    Require(ValidateExtractedDiscDestination(f.source, f.destination / "Default.xexp").empty(),
            "A disjoint update output was rejected");
    Require(ValidateExtractedDiscDestination(f.source, f.destination / "dlc/package").empty(),
            "A disjoint DLC output was rejected");
    Require(!ValidateExtractedDiscDestination(f.source, f.source / "Default.xexp").empty(),
            "An update output inside the donor was accepted");
    Require(!ValidateExtractedDiscDestination(f.source, {}).empty(),
            "An empty output path was accepted");
  });
  test("linked update output cannot overwrite donor", [] {
    Fixture f;
    fs::create_directories(f.destination);
    fs::create_hard_link(f.source / "Default.xex", f.destination / "Default.xexp");
    Require(!ValidateExtractedDiscDestination(f.source, f.destination / "Default.xexp").empty(),
            "An update output hard link was accepted");
    Require(Read(f.source / "Default.xex") == "test executable bytes", "Donor changed");
  });
#if defined(_WIN32)
  test("Windows case-insensitive module and overlap paths", [] {
    Fixture f;
    fs::rename(f.source / "Default.xex", f.source / "default.XEX");
    fs::rename(f.source / "Data/GameLogic.dll", f.source / "Data/gamelogic.DLL");
    Require(ValidateExtractedDisc(f.source).empty(), "Windows module case alias rejected");
    fs::path alias =
        f.source.parent_path() / fs::path(u8"DISC WITH SPACES \u0414\u043e\u043d\u043e\u0440");
    RejectPlan(f.source, alias);
    Write(f.source / "$systemupdate/update.bin", "do not install");
    Require(!Has(Inventory(f.source), "$systemupdate/update.bin"),
            "Windows update case alias included");
  });
  test("Windows source junction rejected", [] {
    Fixture f;
    MakeJunction(f.source, f.root / "source junction");
    RejectPlan(f.root / "source junction", f.destination);
  });
  test("Windows nested source junction rejected", [] {
    Fixture f;
    fs::create_directories(f.root / "other assets");
    MakeJunction(f.root / "other assets", f.source / "Data/junction");
    RejectPlan(f.source, f.destination);
  });
  test("Windows destination junction ancestor rejected", [] {
    Fixture f;
    fs::create_directories(f.root / "other output");
    MakeJunction(f.root / "other output", f.root / "output junction");
    RejectPlan(f.source, f.root / "output junction/new game");
  });
#endif
  std::cout << "Tests: " << passed << " passed, " << failed << " failed, " << skipped
            << " skipped\n";
  return failed == 0 ? 0 : 1;
}

int Inspect(const fs::path& source) {
  const auto error = ValidateExtractedDisc(source);
  if (!error.empty()) {
    std::cerr << error << '\n';
    return 1;
  }
  const auto files = Inventory(source);
  size_t total = 0;
  for (const auto& file : files) total += file.size;
  std::cout << "Read-only extracted-disc inventory: " << files.size() << " files, " << total
            << " bytes; " << ExtractedDiscFingerprint(source) << '\n';
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 1) return RunTests();
    if (argc == 3 && std::string(argv[1]) == "--inspect") return Inspect(fs::path(argv[2]));
    std::cerr << "Usage: extracted_disc_tests [--inspect extracted-game-folder]\n";
    return 2;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
