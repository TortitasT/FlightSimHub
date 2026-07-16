#pragma once

#include <FSHub/AppDefinition.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace FSHub {

struct InstallState {
  enum class Status { Detected, LocatedManually, NotFound };
  Status status {Status::NotFound};
  std::filesystem::path exePath;
};

class IProbe {
 public:
  virtual ~IProbe() = default;
  virtual std::optional<std::string> ReadRegistryString(
    const std::string& root,
    const std::string& path,
    const std::string& value) const = 0;
  virtual bool FileExists(const std::filesystem::path&) const = 0;
  virtual std::string ExpandEnvironmentVars(const std::string&) const = 0;
  virtual std::optional<std::string> ReadTextFile(
    const std::filesystem::path&) const = 0;
  virtual std::vector<std::filesystem::path> ListSubdirectories(
    const std::filesystem::path&) const = 0;
};

std::vector<std::filesystem::path> ParseSteamLibraryFolders(
  std::string_view vdfText);

class Detector {
 public:
  Detector(const IProbe& probe, std::filesystem::path managedAppsDir);

  // Priority: user override, registry hints, steam libraries,
  // well-known paths, managed apps dir.
  InstallState Resolve(
    const AppDefinition& app,
    const std::optional<std::filesystem::path>& userOverride) const;

 private:
  const std::vector<std::filesystem::path>& SteamCommonDirs() const;

  const IProbe& mProbe;
  std::filesystem::path mManagedAppsDir;
  // Steam config is identical for every app in a scan; resolve it once
  mutable std::optional<std::vector<std::filesystem::path>> mSteamCommonDirs;
};

}  // namespace FSHub
