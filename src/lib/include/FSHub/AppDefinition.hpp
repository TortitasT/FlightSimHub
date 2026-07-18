#pragma once

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <vector>

namespace FSHub {

enum class AppKind { Sim, Companion };
enum class SourceType { GitHub, Manual };
enum class InstallKind { Installer, Portable, None };

struct RegistryHint {
  std::string root;  // "HKCU" or "HKLM"
  std::string path;
  std::string value;
  // Appended when the registry value is a directory rather than the exe itself
  std::string append;
};

struct Detection {
  std::vector<RegistryHint> registryKeys;
  std::vector<std::string> wellKnownPaths;  // may contain %ENV% variables
  std::optional<std::string> steamRelativeExe;  // under <library>/steamapps/common/
};

struct Source {
  SourceType type {SourceType::Manual};
  std::string repo;  // github only, "owner/repo"
  std::string assetPattern;  // github only, ECMAScript regex on the asset name
  InstallKind installKind {InstallKind::None};
  std::string homepage;  // manual only
};

struct AppDefinition {
  std::string id;
  std::string name;
  std::string exeName;
  AppKind kind {AppKind::Companion};
  Detection detection;
  Source source;
  // Companions only: which Environment section this app belongs under — a sim's
  // id (e.g. "falcon-bms") for sim-specific tools, or "general" for cross-sim
  // tools. Empty is treated as "general". Sims form their own section.
  std::string group;
  // Companions only: the accessible name (prefix) of the app's "start tracking"
  // button, e.g. "Start". When set, the app supports auto-starting tracking on
  // launch: the engine invokes that button via UI Automation. Empty means the
  // app has no such capability.
  std::string startTrackingButton;
  // Sims only: when the launched exe is a launcher that spawns the game as a
  // separate process, the game's process name (e.g. "Falcon BMS.exe"). The
  // engine monitors this process, not the launcher, to detect the game
  // closing. Empty means the launched process is the game itself.
  std::string gameProcessName;
};

// Throws std::runtime_error naming the offending entry and field.
std::vector<AppDefinition> ParseCatalog(const nlohmann::json& catalog);

}  // namespace FSHub
