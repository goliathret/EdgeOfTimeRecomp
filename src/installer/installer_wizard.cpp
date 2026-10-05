#include "installer/installer_wizard.h"
#include "platform/display.h"

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/filesystem/devices/disc_image_device.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <functional>

#include "core/encoding.h"
#include "core/logging.h"
#include "embedded.h"
#include "installer/extracted_disc.h"
#include "installer/self_install.h"
#include "platform/file_dialog.h"
#include "platform/process.h"
#include "ui/theme.h"

REXCVAR_DEFINE_STRING(eot_install_disc, "", "EdgeOfTime/Config", "Disc image or extracted game folder to install");
REXCVAR_DEFINE_STRING(eot_install_update, "", "EdgeOfTime/Config", "Title update to install");
REXCVAR_DEFINE_STRING(eot_install_dlc, "", "EdgeOfTime/Config", "DLC packages to install");
REXCVAR_DEFINE_STRING(eot_install_dir, "", "EdgeOfTime/Config", "Folder to install into");
REXCVAR_DEFINE_BOOL(eot_install_unattended, false, "EdgeOfTime/Config", "Install without any clicks");

namespace eot::installer {
namespace {

ImFont *g_body_font = nullptr;
ImFont *g_title_font = nullptr;
ImFont *g_path_font = nullptr;

constexpr const char *kTitleMain = "reeot - Installer";
constexpr const char *kTitleRepair = "reeot - Repair";
constexpr const char *kRepairNotice =
    "Existing install found. Continue keeps it as it is and adds any packages listed; Repair takes the disc "
    "image or extracted game folder and the title update again and copies back only what is missing.";
constexpr const char *kSpaceHint = "(~6 GB required)";

constexpr const char *kSuggestedVsync = "false";
constexpr const char *kSuggestedQuality = "low";

struct SettingRow {
  const char *label;
  const char *cvar;
  std::vector<std::pair<std::string, std::string>> values;
};

std::vector<SettingRow> g_rows;

void BuildRows(const eot::platform::Display &display) {
  g_rows = {
      {"Quality preset", "eot_quality_preset", {{"Orig", "low"}, {"Medium", "medium"}, {"High", "high"}}},
      {"Display mode", "fullscreen", {{"Windowed", "false"}, {"Fullscreen", "true"}}},
      {"Resolution",
       "eot_resolution",
       {{"720p", "720p"}, {"1080p", "1080p"}, {"1440p", "1440p"}, {"2160p (4K)", "2160p"}}},
      {"Aspect ratio",
       "eot_aspect_ratio",
       {{"4:3", "4:3"}, {"16:10", "16:10"}, {"16:9", "16:9"}, {"21:9", "21:9"}, {"32:9", "32:9"}}},
  };
  SettingRow frame_rate{"Frame rate limit", "eot_fps_limit", {}};
  std::vector<uint32_t> rates = {30, 60, 120};
  const uint32_t own = eot::platform::AutoFrameRateLimit(display);
  if (std::find(rates.begin(), rates.end(), own) == rates.end())
    rates.push_back(own);
  std::sort(rates.begin(), rates.end());
  for (const uint32_t rate : rates)
    frame_rate.values.push_back(
        {std::to_string(rate) + (rate == own ? " fps (display)" : " fps"), std::to_string(rate)});
  frame_rate.values.push_back({"Unlimited", "0"});
  g_rows.push_back(std::move(frame_rate));
  g_rows.push_back({"Vsync", "eot_vsync", {{"Off", "false"}, {"On", "true"}}});
  g_rows.push_back({"Language",
                    "eot_language",
                    {{"Auto", "auto"},
                     {"English", "en"},
                     {"Fran\xc3\xa7" "ais", "fr"},
                     {"Italiano", "it"},
                     {"Deutsch", "de"},
                     {"Espa\xc3\xb1" "ol", "es"}}});
}

int RowSelected(const SettingRow &row) {
  const std::string current = rex::cvar::GetFlagByName(row.cvar);
  for (size_t i = 0; i < row.values.size(); ++i)
    if (current == row.values[i].second)
      return static_cast<int>(i);
  return -1;
}

void DrawTitle(const char *text) {
  if (g_title_font)
    ImGui::PushFont(g_title_font);
  ImGui::TextUnformatted(text);
  if (g_title_font)
    ImGui::PopFont();
}

std::string ShownName(const std::string &utf8) {
  std::string out;
  for (size_t i = 0; i < utf8.size();) {
    const auto lead = static_cast<unsigned char>(utf8[i]);
    const size_t length = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
    if (length <= 2)
      out.append(utf8, i, length);
    i += length;
  }
  return out;
}

void SectionHeader(const char *text) {
  ImGui::TextUnformatted(text);
  ImGui::Separator();
  ImGui::Spacing();
}

void FilenameCell(const std::filesystem::path &path) {
  if (path.empty()) {
    ImGui::TextDisabled("not selected");
    return;
  }
  ImGui::TextUnformatted(path.filename().string().c_str());
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", path.string().c_str());
}

void StatusCell(bool valid, const std::string &status) {
  if (status.empty())
    return;
  const ImVec4 color = valid ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(color, "%s", status.c_str());
}

void SourceRow(const char *id, const char *button, const std::filesystem::path &path, const char *empty_hint,
               bool valid, const std::string &status, const std::function<void()> &on_pick) {
  ImGui::PushID(id);
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  if (ImGui::Button(button, ImVec2(-FLT_MIN, 0)))
    on_pick();
  ImGui::TableSetColumnIndex(1);
  ImGui::AlignTextToFramePadding();
  if (path.empty())
    ImGui::TextDisabled("%s", empty_hint);
  else
    FilenameCell(path);
  ImGui::TableSetColumnIndex(2);
  StatusCell(valid, status);
  ImGui::PopID();
}

void DirectoryRow(const char *heading, const char *sublabel, const std::filesystem::path &path, const char *id,
                  const std::function<void()> &on_change) {
  ImGui::PushID(id);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(heading);
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, ui::Theme::White(0.55f));
  ImGui::TextUnformatted(sublabel);
  ImGui::PopStyleColor();
  ImGui::SameLine();
  if (ImGui::Button("Change"))
    on_change();

  if (g_path_font)
    ImGui::PushFont(g_path_font);
  ImGui::Indent(12.0f);
  if (path.empty())
    ImGui::TextDisabled("not selected");
  else
    ImGui::TextWrapped("%s", path.string().c_str());
  ImGui::Unindent(12.0f);
  if (g_path_font)
    ImGui::PopFont();
  ImGui::PopID();
}

constexpr float kLabelColumn = 150.0f;
constexpr float kValueWidth = 190.0f;

void SettingCells(const SettingRow &row, const std::function<void(const SettingRow &, int)> &pick) {
  ImGui::PushID(row.cvar);
  ImGui::TableNextColumn();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(row.label);
  ImGui::TableNextColumn();
  const int count = static_cast<int>(row.values.size());
  const int selected = RowSelected(row);
  const char *current = selected >= 0 ? row.values[static_cast<size_t>(selected)].first.c_str() : "";
  ImGui::SetNextItemWidth(kValueWidth);
  if (ImGui::BeginCombo("##value", current)) {
    for (int i = 0; i < count; ++i) {
      ImGui::PushID(i);
      if (ImGui::Selectable(row.values[static_cast<size_t>(i)].first.c_str(), i == selected))
        pick(row, i);
      if (i == selected)
        ImGui::SetItemDefaultFocus();
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  ImGui::PopID();
}

}

void InitInstallerFonts(ImFontAtlas *atlas) {
  ImFontConfig cfg;
  cfg.FontDataOwnedByAtlas = false;
  cfg.OversampleH = 2;
  cfg.OversampleV = 2;
  auto load = [&](float px) {
    constexpr auto kFont = eot::Embedded("fonts/HelveticaNeueRoman.otf");
    return atlas->AddFontFromMemoryTTF(const_cast<uint8_t *>(kFont.data), static_cast<int>(kFont.size), px,
                                       &cfg);
  };
  g_body_font = load(18.0f);
  g_title_font = load(40.0f);
  g_path_font = load(13.0f);
  if (!g_body_font)
    EOT_WARN("[install] the installer font did not load; the wizard uses the drawer default");
}

InstallerWizard::InstallerWizard(rex::ui::ImGuiDrawer *drawer, rex::ui::ImmediateDrawer *immediate_drawer,
                                 rex::ui::WindowedAppContext &app_context,
                                 const std::filesystem::path &default_install_dir, bool repair,
                                 const InstallConfig *existing, CompletionCallback on_done)
    : ImGuiDialog(drawer), app_context_(app_context), immediate_drawer_(immediate_drawer),
      on_done_(std::move(on_done)), repair_(repair), install_dir_(default_install_dir) {
  if (existing)
    disc_fingerprint_ = existing->disc_fingerprint;
  missing_program_files_ = MissingProgramFiles();
  if (!missing_program_files_.empty())
    EOT_ERROR("[install] {}", MissingProgramFilesLine());
  SuggestDefaults();
  Prefill();
  music_.Start();
}

std::string InstallerWizard::MissingProgramFilesLine() const {
  if (missing_program_files_.empty())
    return {};
  std::string line = std::string("Missing beside ") + eot::platform::kExecutableFileName + ": ";
  for (size_t i = 0; i < missing_program_files_.size(); ++i)
    line += (i ? ", " : "") + missing_program_files_[i];
  return line + ". Copy the whole release folder.";
}

void InstallerWizard::SuggestDefaults() {
  const eot::platform::Display display = eot::platform::DisplayFor(nullptr);
  BuildRows(display);
  auto suggest = [](const char *cvar, const std::string &value) {
    if (rex::cvar::GetFlagSource(cvar) == rex::cvar::Source::kDefault)
      rex::cvar::SetFlagByName(cvar, value);
  };
  EOT_INFO("[install] primary display {}x{} at {} Hz: suggesting {}, {}, {} fps, the {} preset", display.width,
           display.height, display.refresh_hz, eot::platform::AutoResolutionPreset(display),
           eot::platform::AutoAspectPreset(display), eot::platform::AutoFrameRateLimit(display),
           kSuggestedQuality);
  suggest("eot_quality_preset", kSuggestedQuality);
  suggest("eot_resolution", eot::platform::AutoResolutionPreset(display));
  suggest("eot_aspect_ratio", eot::platform::AutoAspectPreset(display));
  suggest("eot_fps_limit", std::to_string(eot::platform::AutoFrameRateLimit(display)));
  suggest("eot_vsync", kSuggestedVsync);
}

void InstallerWizard::RecordSettings() {
  choices_.settings.clear();
  for (const SettingRow &row : g_rows)
    choices_.settings.push_back({row.cvar, rex::cvar::GetFlagByName(row.cvar)});
  std::string pip = rex::cvar::GetFlagByName("eot_pip_scale");
  if (rex::cvar::GetFlagSource("eot_pip_scale") == rex::cvar::Source::kDefault) {
    const std::string resolution = rex::cvar::GetFlagByName("eot_resolution");
    pip = (resolution == "720p" || resolution == "native") ? "66" : "50";
  }
  choices_.settings.push_back({"eot_pip_scale", pip});
}

void InstallerWizard::Prefill() {
  const std::string disc = REXCVAR_GET(eot_install_disc);
  if (!disc.empty()) {
    disc_path_ = disc;
    ValidateDisc();
  }
  const std::string update = REXCVAR_GET(eot_install_update);
  if (!update.empty()) {
    update_path_ = update;
    ValidateUpdate();
  }
  const std::string dlc = REXCVAR_GET(eot_install_dlc);
  size_t start = 0;
  while (start <= dlc.size()) {
    const size_t sep = dlc.find(';', start);
    const std::string one = dlc.substr(start, sep == std::string::npos ? std::string::npos : sep - start);
    if (!one.empty())
      AddDlc(one);
    if (sep == std::string::npos)
      break;
    start = sep + 1;
  }
  const std::string dir = REXCVAR_GET(eot_install_dir);
  if (!dir.empty())
    install_dir_ = InstallRootFor(dir);
  unattended_ = REXCVAR_GET(eot_install_unattended) && !disc.empty();
  if (unattended_)
    EOT_INFO("[install] unattended install from {} into {}", disc, install_dir_.string());
}

void InstallerWizard::AddDlc(const std::filesystem::path &path) {
  for (const auto &d : dlc_)
    if (d.path == path)
      return;
  DlcEntry entry;
  entry.path = path;
  const PackageInfo info = InspectPackage(entry.path);
  const std::string why = info.ok ? CheckDlcPackage(info) : info.error;
  entry.valid = why.empty();
  entry.name = info.display_name.empty() ? entry.path.filename().string() : ShownName(info.display_name);
  entry.status = entry.valid ? "Valid" : why;
  dlc_.push_back(std::move(entry));
}

InstallerWizard::~InstallerWizard() {
  if (install_thread_.joinable()) {
    progress_.canceled.store(true);
    install_thread_.join();
  }
}

void InstallerWizard::Finish(bool completed) {
  if (finished_)
    return;
  finished_ = true;
  music_.Stop();

  InstallConfig cfg;
  cfg.install_root = std::filesystem::absolute(install_dir_);
  cfg.disc_fingerprint = disc_fingerprint_;

  EOT_INFO("[install] wizard finished, completed={}", completed);

  REXCVAR_SET(eot_install_disc, "");
  REXCVAR_SET(eot_install_update, "");
  REXCVAR_SET(eot_install_dlc, "");
  REXCVAR_SET(eot_install_dir, "");
  REXCVAR_SET(eot_install_unattended, false);

  auto cb = on_done_;
  const WizardChoices choices = choices_;
  app_context_.CallInUIThreadDeferred([cb, completed, cfg, choices]() { cb(completed, cfg, choices); });
}

void InstallerWizard::ValidateDisc() {
  disc_valid_ = false;
  disc_fingerprint_.clear();
  if (disc_path_.empty()) {
    disc_status_.clear();
    return;
  }
  std::error_code ec;
  if (std::filesystem::is_directory(disc_path_, ec)) {
    disc_status_ = ValidateExtractedDisc(disc_path_);
    if (!disc_status_.empty())
      return;
    disc_fingerprint_ = ExtractedDiscFingerprint(disc_path_);
    disc_valid_ = !disc_fingerprint_.empty();
    disc_status_ = disc_valid_ ? "Valid extracted folder" : "Could not read the extracted game folder";
    return;
  }
  auto disc = OpenDiscImage(disc_path_);
  if (!disc) {
    disc_status_ = "Could not read as Xbox 360 disc image";
    return;
  }
  if (!installer::ValidateDisc(*disc)) {
    disc_status_ = "Not the Edge of Time disc";
    return;
  }
  disc_valid_ = true;
  disc_fingerprint_ = DiscFingerprint(disc_path_, *disc);
  disc_status_ = "Valid";
}

void InstallerWizard::ValidateUpdate() {
  update_valid_ = false;
  if (update_path_.empty()) {
    update_status_.clear();
    return;
  }
  const PackageInfo info = InspectPackage(update_path_);
  const std::string why = info.ok ? CheckUpdatePackage(info) : info.error;
  if (!why.empty()) {
    update_status_ = why;
    return;
  }
  update_valid_ = true;
  update_status_ = "Valid";
}

bool InstallerWizard::InputsReady() const {
  if (!missing_program_files_.empty())
    return false;
  if (!disc_valid_ || !update_valid_ || install_dir_.empty())
    return false;
  for (const auto &d : dlc_)
    if (!d.valid)
      return false;
  return true;
}

bool InstallerWizard::CanContinue() const {
  if (!repair_ || !missing_program_files_.empty() || install_dir_.empty())
    return false;
  std::error_code ec;
  if (!std::filesystem::is_directory(install_dir_, ec))
    return false;
  for (const auto &d : dlc_)
    if (!d.valid)
      return false;
  return true;
}

void InstallerWizard::PickDisc() {
  const platform::FileFilter kFilters[] = {
      {L"Xbox 360 disc image", L"*.iso"},
      {L"All files", L"*.*"},
  };
  auto picked = platform::ShowOpenFileDialog(L"Select Xbox 360 Disc Image", kFilters);
  if (!picked)
    return;
  disc_path_ = *picked;
  ValidateDisc();
}

void InstallerWizard::PickExtractedFolder() {
  auto picked = platform::ShowOpenFolderDialog(L"Select Extracted Game Folder");
  if (!picked)
    return;
  disc_path_ = *picked;
  ValidateDisc();
}

void InstallerWizard::PickUpdate() {
  const platform::FileFilter kFilters[] = {
      {L"Title update package", L"*.*"},
  };
  auto picked = platform::ShowOpenFileDialog(L"Select the Title Update Package", kFilters);
  if (!picked)
    return;
  update_path_ = *picked;
  ValidateUpdate();
}

void InstallerWizard::PickDlc() {
  const platform::FileFilter kFilters[] = {
      {L"Content package", L"*.*"},
  };
  auto picked = platform::ShowOpenFileDialog(L"Select a Content Package", kFilters);
  if (!picked)
    return;
  AddDlc(*picked);
}

void InstallerWizard::PickInstallDir() {
  auto picked = platform::ShowOpenFolderDialog(L"Select Install Location");
  if (!picked)
    return;
  install_dir_ = InstallRootFor(*picked);
  install_status_.clear();
}

void InstallerWizard::ContinueRepair() {
  bool packages = false;
  for (const auto &d : dlc_)
    packages = packages || d.valid;
  if (!packages) {
    RecordSettings();
    EOT_INFO("[install] repair: continuing with the install as it is");
    Finish(true);
    return;
  }
  StartInstall(false);
}

void InstallerWizard::StartInstall(bool disc_and_update) {
  progress_.files_done.store(0);
  progress_.files_total.store(0);
  progress_.bytes_done.store(0);
  progress_.bytes_total.store(0);
  progress_.complete.store(false);
  progress_.failed.store(false);
  progress_.canceled.store(false);
  progress_.SetCurrentFile("");
  progress_.SetError("");
  done_message_.clear();
  done_success_ = false;
  install_status_.clear();
  page_ = Page::Installing;
  RecordSettings();

  const auto abs_install = std::filesystem::absolute(install_dir_);
  const auto abs_game = abs_install / "game";
  EOT_INFO("[install] install root -> '{}'", abs_install.string());
  EOT_INFO("[install]   game data  -> '{}'", abs_game.string());

  InstallSources sources;
  if (disc_and_update) {
    sources.disc = disc_path_;
    sources.update = update_path_;
  }
  for (const auto &d : dlc_)
    sources.dlc.push_back(d.path);

  try {
    install_thread_ = Installer::RunAsync(sources, abs_game, repair_, progress_);
  } catch (const std::system_error &e) {
    EOT_ERROR("[install] could not start the install thread: {}", e.what());
    progress_.SetError(std::string("Failed to start install: ") + e.what());
    progress_.failed.store(true);
    progress_.complete.store(true);
  }
}

void InstallerWizard::OnDraw(ImGuiIO &io) {
  music_.Update(io.DeltaTime);
  shown_seconds_ += io.DeltaTime;
  if (unattended_ && page_ == Page::Main && !finished_) {
    if (InputsReady()) {
      StartInstall();
    } else {
      EOT_ERROR("[install] unattended install cannot start: disc {} update {} dir {} {}", disc_status_,
                update_status_, install_dir_.string(), MissingProgramFilesLine());
      unattended_ = false;
    }
  }
  if (unattended_ && page_ == Page::Done)
    Finish(done_success_);

  if (!background_texture_ && !background_tried_ && immediate_drawer_) {
    background_tried_ = true;
    int w = 0, h = 0, channels = 0;
    constexpr auto kImage = eot::Embedded("installer/installer.png");
    uint8_t *rgba =
        stbi_load_from_memory(kImage.data, static_cast<int>(kImage.size), &w, &h, &channels, 4);
    if (!rgba) {
      EOT_ERROR("[install] the background image did not decode: {}", stbi_failure_reason());
    } else {
      background_texture_ =
          immediate_drawer_->CreateTexture(static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                                           rex::ui::ImmediateTextureFilter::kLinear, false, rgba);
      stbi_image_free(rgba);
      if (!background_texture_)
        EOT_ERROR("[install] the background texture could not be created");
    }
  }

  auto *vp = ImGui::GetMainViewport();
  constexpr float kFadeInSeconds = 2.0f;
  if (shown_seconds_ < kFadeInSeconds) {
    const float t = shown_seconds_ / kFadeInSeconds;
    const float alpha = (1.0f - t) * (1.0f - t);
    ImGui::GetForegroundDrawList()->AddRectFilled(
        vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
        IM_COL32(0, 0, 0, static_cast<int>(alpha * 255.0f)));
  }
  ImGui::SetNextWindowPos(vp->WorkPos);
  ImGui::SetNextWindowSize(vp->WorkSize);
  ImGui::SetNextWindowBgAlpha(0.0f);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
  if (ImGui::Begin("##installer", nullptr, flags)) {
    if (background_texture_) {
      const ImVec2 p0 = vp->WorkPos;
      const ImVec2 p1(p0.x + vp->WorkSize.x, p0.y + vp->WorkSize.y);
      ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(background_texture_.get()), p0, p1);
    }

    constexpr float kPanelMaxWidth = 1040.0f;
    constexpr float kPanelMinWidth = 420.0f;
    constexpr float kPanelMargin = 32.0f;
    const float panel_width =
        std::clamp(vp->WorkSize.x - kPanelMargin * 2.0f, kPanelMinWidth, kPanelMaxWidth);
    ImGui::SetCursorPos(ImVec2(kPanelMargin, kPanelMargin));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::Theme::kPanel);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 7));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 8));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, vp->WorkSize.y - kPanelMargin * 2.0f));
    if (ImGui::BeginChild("##installer_panel", ImVec2(panel_width, 0),
                          ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoSavedSettings)) {
      if (g_body_font)
        ImGui::PushFont(g_body_font);
      switch (page_) {
      case Page::Main:
        DrawMain();
        break;
      case Page::Installing:
        DrawInstalling();
        break;
      case Page::Done:
        DrawDone();
        break;
      }
      if (g_body_font)
        ImGui::PopFont();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor();
  }
  ImGui::End();
}

void InstallerWizard::DrawMain() {
  DrawTitle(repair_ ? kTitleRepair : kTitleMain);
  ImGui::Spacing();

  if (repair_) {
    ImGui::TextWrapped("%s", kRepairNotice);
    ImGui::Spacing();
  }
  if (!missing_program_files_.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
    ImGui::TextWrapped("%s", MissingProgramFilesLine().c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();
  }

  DrawSources();

  ImGui::Dummy(ImVec2(0, 10));
  DrawDlcSection();

  ImGui::Dummy(ImVec2(0, 10));
  SectionHeader("Install Folder");
  DirectoryRow("Location", repair_ ? "(existing install)" : kSpaceHint, install_dir_, "install_dir",
               [this]() { PickInstallDir(); });

  ImGui::Dummy(ImVec2(0, 10));
  DrawSettings();

  DrawFooter();
}

void InstallerWizard::DrawSources() {
  SectionHeader("Sources");

  const ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody;
  if (!ImGui::BeginTable("##inputs", 3, flags))
    return;
  ImGui::TableSetupColumn("##btn", ImGuiTableColumnFlags_WidthFixed, 170.0f);
  ImGui::TableSetupColumn("##path", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("##status", ImGuiTableColumnFlags_WidthFixed, 260.0f);

  std::error_code ec;
  const bool extracted_folder = std::filesystem::is_directory(disc_path_, ec);
  const std::filesystem::path unselected;
  SourceRow("disc", "Disc Image...", extracted_folder ? unselected : disc_path_,
            "or select an extracted folder", !extracted_folder && disc_valid_,
            extracted_folder ? "" : disc_status_,
            [this]() { PickDisc(); });
  SourceRow("disc_folder", "Extracted Folder...", extracted_folder ? disc_path_ : unselected,
            "or select a disc image", extracted_folder && disc_valid_,
            extracted_folder ? disc_status_ : "",
            [this]() { PickExtractedFolder(); });
  SourceRow("update", "Title Update...", update_path_, "required", update_valid_, update_status_,
            [this]() { PickUpdate(); });

  ImGui::EndTable();
}

void InstallerWizard::DrawDlcSection() {
  SectionHeader("Downloadable Content");

  if (dlc_.empty()) {
    ImGui::TextDisabled("None selected.");
  } else {
    const char *remove_label = "Remove";
    const float remove_width =
        ImGui::CalcTextSize(remove_label).x + ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 5));
    if (ImGui::BeginTable("##dlc", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody)) {
      ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("##status", ImGuiTableColumnFlags_WidthFixed, 260.0f);
      ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, remove_width);
      for (size_t i = 0; i < dlc_.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(dlc_[i].name.c_str());
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("%s", dlc_[i].path.string().c_str());
        ImGui::TableSetColumnIndex(1);
        StatusCell(dlc_[i].valid, dlc_[i].status);
        ImGui::TableSetColumnIndex(2);
        if (ImGui::Button(remove_label, ImVec2(remove_width, 0))) {
          dlc_.erase(dlc_.begin() + static_cast<long>(i));
          ImGui::PopID();
          break;
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    ImGui::PopStyleVar();
  }

  ImGui::Spacing();
  if (ImGui::Button("Add Package...", ImVec2(160, 0)))
    PickDlc();
}

void InstallerWizard::DrawSettings() {
  SectionHeader("Settings");

  auto pick = [](const SettingRow &row, int i) {
    rex::cvar::SetFlagByName(row.cvar, row.values[static_cast<size_t>(i)].second);
  };
  if (ImGui::BeginTable("##settings", 4, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("##l0", ImGuiTableColumnFlags_WidthFixed, kLabelColumn);
    ImGui::TableSetupColumn("##v0", ImGuiTableColumnFlags_WidthFixed, kValueWidth + 30.0f);
    ImGui::TableSetupColumn("##l1", ImGuiTableColumnFlags_WidthFixed, kLabelColumn);
    ImGui::TableSetupColumn("##v1", ImGuiTableColumnFlags_WidthStretch);
    int cell = 0;
    for (const SettingRow &row : g_rows) {
      if (cell % 2 == 0)
        ImGui::TableNextRow();
      SettingCells(row, pick);
      ++cell;
    }
    ImGui::EndTable();
  }

#if defined(_WIN32) || defined(__linux__)
  ImGui::Spacing();
  if (ImGui::Checkbox("Create a desktop shortcut", &create_shortcut_))
    choices_.create_shortcut = create_shortcut_;
#endif
}

void InstallerWizard::DrawFooter() {
  ImGui::Dummy(ImVec2(0, 6));
  ImGui::Separator();
  ImGui::Spacing();

  if (!install_status_.empty()) {
    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "%s", install_status_.c_str());
    ImGui::Spacing();
  }

  constexpr float kButtonWidth = 130.0f;
  constexpr ImVec2 kButton(kButtonWidth, 0);

  if (ImGui::Button("Exit", kButton))
    Finish(false);
  constexpr float kGap = 8.0f;
  const int forward = repair_ ? 2 : 1;
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - forward * kButtonWidth -
                       (forward - 1) * kGap);
  if (repair_) {
    ImGui::BeginDisabled(!CanContinue());
    if (ImGui::Button("Continue", kButton))
      ContinueRepair();
    ImGui::EndDisabled();
    ImGui::SameLine(0, kGap);
  }
  ImGui::BeginDisabled(!InputsReady());
  if (ImGui::Button(repair_ ? "Repair" : "Install", kButton))
    StartInstall();
  ImGui::EndDisabled();
}

void InstallerWizard::DrawInstalling() {
  DrawTitle(repair_ ? "Repairing..." : "Installing...");
  ImGui::Spacing();

  const size_t total_bytes = progress_.bytes_total.load();
  const size_t done_bytes = progress_.bytes_done.load();
  const float fraction =
      total_bytes == 0 ? 0.0f : static_cast<float>(done_bytes) / static_cast<float>(total_bytes);
  ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0), nullptr);

  auto format_bytes = [](size_t bytes) {
    constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
    constexpr double kMiB = 1024.0 * 1024.0;
    char buf[32];
    const double b = static_cast<double>(bytes);
    if (b >= kGiB)
      std::snprintf(buf, sizeof(buf), "%.2f GiB", b / kGiB);
    else
      std::snprintf(buf, sizeof(buf), "%.1f MiB", b / kMiB);
    return std::string(buf);
  };
  ImGui::Text("%s / %s", format_bytes(done_bytes).c_str(), format_bytes(total_bytes).c_str());

  const std::string current = progress_.GetCurrentFile();
  if (!current.empty())
    ImGui::Text("Installing: %s", current.c_str());

  ImGui::Spacing();
  if (ImGui::Button("Cancel", ImVec2(120, 0)))
    progress_.canceled.store(true);

  if (progress_.complete.load()) {
    if (install_thread_.joinable())
      install_thread_.join();
    if (progress_.canceled.load()) {
      install_status_ = "The install was canceled. Check the inputs and click Install to resume.";
      page_ = Page::Main;
    } else if (progress_.failed.load()) {
      done_success_ = false;
      done_message_ = "Install failed: " + progress_.GetError();
      page_ = Page::Done;
    } else {
      done_success_ = true;
      done_message_ = repair_ ? "Repair complete." : "Install complete.";
      page_ = Page::Done;
    }
  }
}

void InstallerWizard::DrawDone() {
  DrawTitle(done_success_ ? "Done" : "Stopped");
  ImGui::Spacing();
  ImGui::TextWrapped("%s", done_message_.c_str());
  ImGui::Spacing();
  if (done_success_) {
    if (ImGui::Button("Continue", ImVec2(120, 0)))
      Finish(true);
  } else {
    if (ImGui::Button("Quit", ImVec2(120, 0)))
      Finish(false);
  }
}

}
