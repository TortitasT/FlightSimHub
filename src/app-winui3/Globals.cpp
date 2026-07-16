#include "pch.h"

#include "Globals.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace FSHub {

AppModel& AppModel::Get() {
  static AppModel instance;
  return instance;
}

void AppModel::Load() {
  wchar_t selfPath[MAX_PATH] {};
  GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
  const auto catalogFile
    = std::filesystem::path(selfPath).parent_path() / "catalog.json";

  std::ifstream stream(catalogFile);
  if (!stream.good()) {
    throw std::runtime_error(
      "catalog.json is missing next to FlightSimHub.exe");
  }
  catalog = ParseCatalog(nlohmann::json::parse(stream));

  const auto dataDir = AppDataDir();
  settingsFile = dataDir / "settings.json";
  managedAppsDir = dataDir / "apps";

  const auto loaded = LoadSettings(settingsFile);
  settings = loaded.settings;
  settingsWereCorrupt = loaded.wasCorrupt;

  Rescan();
}

std::optional<std::filesystem::path> AppModel::OverrideFor(
  const std::string& appId) const {
  const auto it = settings.appOverrides.find(appId);
  if (it == settings.appOverrides.end()) {
    return std::nullopt;
  }
  return std::filesystem::path(it->second);
}

void AppModel::Rescan() {
  const Detector detector(probe, managedAppsDir);
  for (const auto& app: catalog) {
    states[app.id] = detector.Resolve(app, OverrideFor(app.id));
  }
}

void AppModel::Rescan(const std::string& appId) {
  const Detector detector(probe, managedAppsDir);
  for (const auto& app: catalog) {
    if (app.id == appId) {
      states[app.id] = detector.Resolve(app, OverrideFor(app.id));
    }
  }
}

void AppModel::Save() const {
  SaveSettings(settingsFile, settings);
}

}  // namespace FSHub
