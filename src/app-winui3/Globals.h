#pragma once

#include <FSHub/AppDefinition.hpp>
#include <FSHub/Detector.hpp>
#include <FSHub/Settings.hpp>
#include <FSHub/WindowsProbe.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <vector>

namespace FSHub {

// Shared state for both pages and the headless --launch path.
struct AppModel {
  std::vector<AppDefinition> catalog;
  Settings settings;
  bool settingsWereCorrupt {false};
  std::map<std::string, InstallState> states;

  std::filesystem::path settingsFile;
  std::filesystem::path managedAppsDir;
  WindowsProbe probe;

  static AppModel& Get();

  // Loads catalog.json from the exe directory and settings from
  // %LOCALAPPDATA%; throws std::runtime_error if the catalog is unusable.
  void Load();

  // Runs the full detection sweep (registry, steam, filesystem probes).
  // Overrides are passed by copy so this can run on a background thread
  // while the UI keeps mutating settings.
  std::map<std::string, InstallState> ComputeStates(
    const std::map<std::string, std::string>& overrides) const;

  void Rescan();
  void Rescan(const std::string& appId);

  // Shows an error dialog on failure rather than throwing out of UI handlers
  void Save() const;

  std::optional<std::filesystem::path> OverrideFor(
    const std::string& appId) const;
};

}  // namespace FSHub
