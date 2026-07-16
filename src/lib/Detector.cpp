#include <FSHub/Detector.hpp>

#include <regex>

namespace FSHub {

std::vector<std::filesystem::path> ParseSteamLibraryFolders(
  std::string_view vdfText) {
  std::vector<std::filesystem::path> paths;
  static const std::regex pattern {R"rx("path"\s+"([^"]+)")rx"};
  auto begin = std::cregex_iterator(
    vdfText.data(), vdfText.data() + vdfText.size(), pattern);
  for (auto it = begin; it != std::cregex_iterator(); ++it) {
    std::string raw = (*it)[1].str();
    // VDF escapes backslashes
    std::string unescaped;
    unescaped.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
      if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '\\') {
        ++i;
      }
      unescaped.push_back(raw[i]);
    }
    paths.emplace_back(std::filesystem::path(unescaped).generic_string());
  }
  return paths;
}

Detector::Detector(const IProbe& probe, std::filesystem::path managedAppsDir)
  : mProbe(probe), mManagedAppsDir(std::move(managedAppsDir)) {
}

const std::vector<std::filesystem::path>& Detector::SteamCommonDirs() const {
  if (mSteamCommonDirs) {
    return *mSteamCommonDirs;
  }
  mSteamCommonDirs.emplace();
  const auto steamPath
    = mProbe.ReadRegistryString("HKCU", "Software\\Valve\\Steam", "SteamPath");
  if (!steamPath) {
    return *mSteamCommonDirs;
  }
  const auto vdfPath
    = std::filesystem::path(*steamPath) / "steamapps" / "libraryfolders.vdf";
  const auto vdfText = mProbe.ReadTextFile(vdfPath);
  if (!vdfText) {
    return *mSteamCommonDirs;
  }
  for (const auto& library: ParseSteamLibraryFolders(*vdfText)) {
    mSteamCommonDirs->push_back(library / "steamapps" / "common");
  }
  return *mSteamCommonDirs;
}

InstallState Detector::Resolve(
  const AppDefinition& app,
  const std::optional<std::filesystem::path>& userOverride) const {
  if (userOverride && mProbe.FileExists(*userOverride)) {
    return {InstallState::Status::LocatedManually, *userOverride};
  }

  for (const auto& hint: app.detection.registryKeys) {
    const auto value
      = mProbe.ReadRegistryString(hint.root, hint.path, hint.value);
    if (!value) {
      continue;
    }
    auto candidate = std::filesystem::path(*value);
    if (!hint.append.empty()) {
      candidate /= hint.append;
    }
    if (mProbe.FileExists(candidate)) {
      return {InstallState::Status::Detected, candidate};
    }
  }

  if (app.detection.steamRelativeExe) {
    for (const auto& commonDir: SteamCommonDirs()) {
      const auto candidate = commonDir / *app.detection.steamRelativeExe;
      if (mProbe.FileExists(candidate)) {
        return {InstallState::Status::Detected, candidate};
      }
    }
  }

  for (const auto& raw: app.detection.wellKnownPaths) {
    const auto candidate
      = std::filesystem::path(mProbe.ExpandEnvironmentVars(raw));
    if (mProbe.FileExists(candidate)) {
      return {InstallState::Status::Detected, candidate};
    }
  }

  const auto managedRoot = mManagedAppsDir / app.id;
  const auto managed = managedRoot / app.exeName;
  if (mProbe.FileExists(managed)) {
    return {InstallState::Status::Detected, managed};
  }
  // Portable zips often nest everything under a versioned folder
  for (const auto& subdir: mProbe.ListSubdirectories(managedRoot)) {
    const auto nested = subdir / app.exeName;
    if (mProbe.FileExists(nested)) {
      return {InstallState::Status::Detected, nested};
    }
  }

  return {};
}

}  // namespace FSHub
