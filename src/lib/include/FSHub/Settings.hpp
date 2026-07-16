#pragma once

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace FSHub {

struct LauncherItem {
  std::string appId;
  std::string args;
  int delayAfterSeconds {3};
};

struct Launcher {
  std::string id;  // uuid string
  std::string name;
  std::vector<LauncherItem> items;
  bool closeCompanionsOnSimExit {true};
};

struct Settings {
  std::map<std::string, std::string> appOverrides;  // appId -> exe path
  std::vector<Launcher> launchers;
};

void to_json(nlohmann::json&, const LauncherItem&);
void from_json(const nlohmann::json&, LauncherItem&);
void to_json(nlohmann::json&, const Launcher&);
void from_json(const nlohmann::json&, Launcher&);
void to_json(nlohmann::json&, const Settings&);
void from_json(const nlohmann::json&, Settings&);

struct LoadResult {
  Settings settings;
  bool wasCorrupt {false};
};

// Corrupt or unreadable files are renamed to <file>.corrupt and defaults returned.
LoadResult LoadSettings(const std::filesystem::path& file);
// Atomic: writes <file>.tmp then renames over the target.
void SaveSettings(const std::filesystem::path& file, const Settings&);

std::string NewLauncherId();

// %LOCALAPPDATA%\FlightSimHub; created on first call. Windows-only.
std::filesystem::path AppDataDir();

}  // namespace FSHub
