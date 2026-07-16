#include <FSHub/AppDefinition.hpp>

#include <nlohmann/json.hpp>

#include <format>
#include <stdexcept>

namespace FSHub {

namespace {

AppKind ParseAppKind(const std::string& kind) {
  if (kind == "sim") {
    return AppKind::Sim;
  }
  if (kind == "companion") {
    return AppKind::Companion;
  }
  throw std::runtime_error(std::format("unknown kind '{}'", kind));
}

InstallKind ParseInstallKind(const std::string& kind) {
  if (kind == "installer") {
    return InstallKind::Installer;
  }
  if (kind == "portable") {
    return InstallKind::Portable;
  }
  throw std::runtime_error(std::format("unknown installKind '{}'", kind));
}

Detection ParseDetection(const nlohmann::json& json) {
  Detection detection;
  for (const auto& hint: json.value("registryKeys", nlohmann::json::array())) {
    detection.registryKeys.push_back({
      .root = hint.at("root"),
      .path = hint.at("path"),
      .value = hint.at("value"),
      .append = hint.value("append", ""),
    });
  }
  for (const auto& path: json.value("wellKnownPaths", nlohmann::json::array())) {
    detection.wellKnownPaths.push_back(path);
  }
  if (json.contains("steamRelativeExe")) {
    detection.steamRelativeExe = json.at("steamRelativeExe");
  }
  return detection;
}

Source ParseSource(const nlohmann::json& json) {
  Source source;
  const std::string type = json.at("type");
  if (type == "github") {
    source.type = SourceType::GitHub;
    source.repo = json.at("repo");
    source.assetPattern = json.at("assetPattern");
    source.installKind = ParseInstallKind(json.at("installKind"));
  } else if (type == "manual") {
    source.type = SourceType::Manual;
    source.homepage = json.at("homepage");
  } else {
    throw std::runtime_error(std::format("unknown source type '{}'", type));
  }
  return source;
}

}  // namespace

std::vector<AppDefinition> ParseCatalog(const nlohmann::json& catalog) {
  std::vector<AppDefinition> apps;
  for (size_t i = 0; i < catalog.size(); ++i) {
    const auto& entry = catalog.at(i);
    const std::string id
      = entry.contains("id") ? std::string {entry.at("id")} : std::string {};
    if (id.empty()) {
      throw std::runtime_error(
        std::format("catalog entry {} is missing 'id'", i));
    }
    try {
      apps.push_back({
        .id = id,
        .name = entry.at("name"),
        .exeName = entry.at("exeName"),
        .kind = ParseAppKind(entry.at("kind")),
        .detection = ParseDetection(entry.value("detection", nlohmann::json::object())),
        .source = ParseSource(entry.at("source")),
      });
    } catch (const std::exception& e) {
      throw std::runtime_error(
        std::format("catalog entry '{}': {}", id, e.what()));
    }
  }
  return apps;
}

}  // namespace FSHub
