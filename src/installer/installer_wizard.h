// installer/installer_wizard.h
// Copyright Rien Gupta <rgupta9@scu.edu>
// BSD 3-Clause

#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <rex/ui/imgui_dialog.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/windowed_app_context.h>

#include "installer/disc_install.h"
#include "installer/installer_music.h"
#include "installer/install_registry.h"

struct ImFontAtlas;

namespace eot::installer {

void InitInstallerFonts(ImFontAtlas *atlas);

struct SettingPick {
  std::string cvar;
  std::string value;
};

struct WizardChoices {
  std::vector<SettingPick> settings;
  bool create_shortcut = false;
};

class InstallerWizard : public rex::ui::ImGuiDialog {
public:
  using CompletionCallback =
      std::function<void(bool completed, const InstallConfig &cfg, const WizardChoices &choices)>;

  InstallerWizard(rex::ui::ImGuiDrawer *drawer, rex::ui::ImmediateDrawer *immediate_drawer,
                  rex::ui::WindowedAppContext &app_context,
                  const std::filesystem::path &default_install_dir, bool repair,
                  const InstallConfig *existing, CompletionCallback on_done);
  ~InstallerWizard();

protected:
  void OnDraw(ImGuiIO &io) override;

private:
  enum class Page { Main, Installing, Done };

  void DrawMain();
  void DrawSources();
  void DrawDlcSection();
  void DrawSettings();
  void DrawFooter();
  void DrawInstalling();
  void DrawDone();

  void PickDisc();
  void PickExtractedFolder();
  void PickUpdate();
  void PickDlc();
  void PickInstallDir();
  void Prefill();
  void SuggestDefaults();
  void RecordSettings();
  void AddDlc(const std::filesystem::path &path);
  std::string MissingProgramFilesLine() const;
  void ValidateDisc();
  void ValidateUpdate();
  bool InputsReady() const;
  bool CanContinue() const;
  void ContinueRepair();
  void StartInstall(bool disc_and_update = true);
  void Finish(bool completed);

  rex::ui::WindowedAppContext &app_context_;
  rex::ui::ImmediateDrawer *immediate_drawer_;
  CompletionCallback on_done_;
  bool finished_ = false;
  bool repair_ = false;

  std::unique_ptr<rex::ui::ImmediateTexture> background_texture_;
  bool background_tried_ = false;

  Page page_ = Page::Main;

  std::filesystem::path disc_path_;
  bool disc_valid_ = false;
  std::string disc_status_;
  std::string disc_fingerprint_;

  std::filesystem::path update_path_;
  bool update_valid_ = false;
  std::string update_status_;

  struct DlcEntry {
    std::filesystem::path path;
    std::string name;
    bool valid = false;
    std::string status;
  };
  std::vector<DlcEntry> dlc_;

  std::filesystem::path install_dir_;
  std::string install_status_;
  std::vector<std::string> missing_program_files_;

  WizardChoices choices_;
  bool create_shortcut_ = false;
  bool unattended_ = false;

  InstallProgress progress_;
  std::thread install_thread_;

  Music music_;
  float shown_seconds_ = 0.0f;
  std::string done_message_;
  bool done_success_ = false;
};

}
