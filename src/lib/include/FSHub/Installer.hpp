#pragma once

#include <FSHub/AppDefinition.hpp>

#include <nlohmann/json_fwd.hpp>

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace FSHub {

struct ReleaseAsset {
  std::string name;
  std::string downloadUrl;
};

// release: GitHub /releases/latest JSON. Returns the first asset whose
// name matches assetPattern. A leading "(?i)" makes the match
// case-insensitive (std::regex has no inline flags).
std::optional<ReleaseAsset> SelectAsset(
  const nlohmann::json& release, const std::string& assetPattern);

using ProgressCallback = std::function<void(double zeroToOne)>;

// Windows-only network/process/shell operations.
std::expected<nlohmann::json, std::string> FetchLatestReleaseJson(
  const std::string& repo);
std::expected<void, std::string> DownloadFile(
  const std::string& url,
  const std::filesystem::path& destFile,
  const ProgressCallback& onProgress);
std::expected<void, std::string> RunInstallerAndWait(
  const std::filesystem::path& installer);
std::expected<void, std::string> ExtractZip(
  const std::filesystem::path& zipFile, const std::filesystem::path& destDir);

// Full flow: fetch release, select asset, download (progress reported),
// then run the installer or extract into managedAppsDir/<app.id>/.
std::expected<void, std::string> InstallApp(
  const AppDefinition& app,
  const std::filesystem::path& managedAppsDir,
  const ProgressCallback& onProgress);

}  // namespace FSHub
