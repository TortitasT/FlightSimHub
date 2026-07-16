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
  // terminates stragglers after a grace period. Windows-only.
  static std::expected<void, std::string> Run(
    const std::vector<LaunchPlanItem>& plan, bool closeCompanions);
};

}  // namespace FSHub
