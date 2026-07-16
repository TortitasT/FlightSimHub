#include "pch.h"

#include "Globals.h"

#include <FSHub/Strings.hpp>

#include <nlohmann/json.hpp>

#include <wil/win32_helpers.h>

#include <fstream>

namespace FSHub {

namespace {

std::optional<std::filesystem::path> OverridePathFor(
  const std::map<std::string, std::string>& overrides,
  const std::string& appId) {
  const auto it = overrides.find(appId);
  if (it == overrides.end()) {
    return std::nullopt;
  }
  // Stored as UTF-8; path's narrow constructor would decode it as ACP
  return std::filesystem::path(Widen(it->second));
}

}  // namespace

AppModel& AppModel::Get() {
  static AppModel instance;
  return instance;
}

void AppModel::Load() {
  const auto selfPath = wil::GetModuleFileNameW<std::wstring>(nullptr);
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
  return OverridePathFor(settings.appOverrides, appId);
}

std::map<std::string, InstallState> AppModel::ComputeStates(
  const std::map<std::string, std::string>& overrides) const {
  const Detector detector(probe, managedAppsDir);
  std::map<std::string, InstallState> result;
  for (const auto& app: catalog) {
    result[app.id] = detector.Resolve(app, OverridePathFor(overrides, app.id));
  }
  return result;
}

void AppModel::Rescan() {
  states = ComputeStates(settings.appOverrides);
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
  try {
    SaveSettings(settingsFile, settings);
  } catch (const std::exception& e) {
    MessageBoxW(
      nullptr,
      winrt::to_hstring(e.what()).c_str(),
      L"FlightSimHub",
      MB_OK | MB_ICONERROR);
  }
}

}  // namespace FSHub
