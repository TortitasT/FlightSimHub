#pragma once

#include <FSHub/AppDefinition.hpp>
#include <FSHub/Detector.hpp>
#include <FSHub/Settings.hpp>

#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace FSHub {

struct LaunchPlanItem {
  std::string appId;
  std::string name;
  std::string args;
  std::filesystem::path exe;
  int delayAfterSeconds {0};
  bool isSim {false};
  // Sims only: process name to monitor instead of the launched exe, for
  // launchers that spawn the game separately. Empty means monitor the
  // launched process itself.
  std::string gameProcessName;
  // Companions only: if set, the accessible name (prefix) of the button to
  // invoke via UI Automation once the app launches, to auto-start tracking.
  // Empty means do nothing.
  std::string startTrackingButton;
};

// Validates the launcher and resolves exe paths. Errors name the
// offending app. Requires exactly one sim among the items.
std::expected<std::vector<LaunchPlanItem>, std::string> BuildLaunchPlan(
  const Launcher& launcher,
  const std::vector<AppDefinition>& catalog,
  const std::map<std::string, InstallState>& states);

class LauncherEngine {
 public:
  // Blocking: launches items in order honoring delays. If closeCompanions,
  // waits for the sim to exit, posts WM_CLOSE to companion windows, and
  // terminates stragglers after a grace period. When the sim defines a
  // gameProcessName (the launched exe is a launcher that spawns the game
  // separately), it waits for that game process to close rather than the
  // launcher, and leaves companions running if the game never started.
  // Windows-only.
  static std::expected<void, std::string> Run(
    const std::vector<LaunchPlanItem>& plan, bool closeCompanions);
};

}  // namespace FSHub
