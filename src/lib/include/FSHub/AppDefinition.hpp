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
};

// Throws std::runtime_error naming the offending entry and field.
std::vector<AppDefinition> ParseCatalog(const nlohmann::json& catalog);

}  // namespace FSHub
