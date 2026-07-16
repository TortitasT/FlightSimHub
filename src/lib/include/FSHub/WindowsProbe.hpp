#pragma once

#include <FSHub/Detector.hpp>

namespace FSHub {

// Live Win32 implementation; keep logic in Detector so this stays a thin shim.
class WindowsProbe final : public IProbe {
 public:
  std::optional<std::string> ReadRegistryString(
    const std::string& root,
    const std::string& path,
    const std::string& value) const override;
  bool FileExists(const std::filesystem::path&) const override;
  std::string ExpandEnvironmentVars(const std::string&) const override;
  std::optional<std::string> ReadTextFile(
    const std::filesystem::path&) const override;
  std::vector<std::filesystem::path> ListSubdirectories(
    const std::filesystem::path&) const override;
};

}  // namespace FSHub
