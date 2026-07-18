#include <FSHub/Settings.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <random>

#ifdef _WIN32
#include <windows.h>

#include <objbase.h>
#include <shlobj.h>
#endif

namespace FSHub {

void to_json(nlohmann::json& json, const LauncherItem& item) {
  json = {
    {"appId", item.appId},
    {"args", item.args},
    {"delayAfterSeconds", item.delayAfterSeconds},
    {"startTracking", item.startTracking},
  };
}

void from_json(const nlohmann::json& json, LauncherItem& item) {
  item.appId = json.at("appId");
  item.args = json.value("args", "");
  item.delayAfterSeconds = json.value("delayAfterSeconds", 3);
  item.startTracking = json.value("startTracking", true);
}

void to_json(nlohmann::json& json, const Launcher& launcher) {
  json = {
    {"id", launcher.id},
    {"name", launcher.name},
    {"items", launcher.items},
    {"closeCompanionsOnSimExit", launcher.closeCompanionsOnSimExit},
  };
}

void from_json(const nlohmann::json& json, Launcher& launcher) {
  launcher.id = json.at("id");
  launcher.name = json.at("name");
  launcher.items = json.value("items", std::vector<LauncherItem> {});
  launcher.closeCompanionsOnSimExit
    = json.value("closeCompanionsOnSimExit", true);
}

void to_json(nlohmann::json& json, const Settings& settings) {
  json = {
    {"appOverrides", settings.appOverrides},
    {"launchers", settings.launchers},
  };
}

void from_json(const nlohmann::json& json, Settings& settings) {
  settings.appOverrides
    = json.value("appOverrides", std::map<std::string, std::string> {});
  settings.launchers = json.value("launchers", std::vector<Launcher> {});
}

LoadResult LoadSettings(const std::filesystem::path& file) {
  std::error_code ec;
  if (!std::filesystem::exists(file, ec)) {
    return {};
  }
  try {
    std::ifstream stream(file);
    return {.settings = nlohmann::json::parse(stream).get<Settings>()};
  } catch (const std::exception&) {
    auto corrupt = file;
    corrupt += ".corrupt";
    std::filesystem::remove(corrupt, ec);
    std::filesystem::rename(file, corrupt, ec);
    return {.wasCorrupt = true};
  }
}

void SaveSettings(const std::filesystem::path& file, const Settings& settings) {
  std::filesystem::create_directories(file.parent_path());
  auto temp = file;
  temp += ".tmp";
  {
    std::ofstream stream(temp);
    stream << nlohmann::json(settings).dump(2);
    stream.flush();
    if (!stream.good()) {
      // Renaming a truncated temp file would destroy the last good
      // settings, the exact loss the temp+rename dance exists to prevent
      std::error_code ec;
      std::filesystem::remove(temp, ec);
      throw std::runtime_error("failed to write settings.json");
    }
  }
  std::filesystem::rename(temp, file);
}

std::string NewLauncherId() {
#ifdef _WIN32
  GUID guid {};
  if (SUCCEEDED(CoCreateGuid(&guid))) {
    char buffer[64] {};
    std::snprintf(
      buffer,
      sizeof(buffer),
      "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      guid.Data1,
      guid.Data2,
      guid.Data3,
      guid.Data4[0],
      guid.Data4[1],
      guid.Data4[2],
      guid.Data4[3],
      guid.Data4[4],
      guid.Data4[5],
      guid.Data4[6],
      guid.Data4[7]);
    return buffer;
  }
#endif
  // Fallback for non-Windows unit test runs; not RFC 4122 compliant
  static std::mt19937_64 rng {std::random_device {}()};
  char buffer[64] {};
  std::snprintf(
    buffer,
    sizeof(buffer),
    "%016llx-%016llx",
    static_cast<unsigned long long>(rng()),
    static_cast<unsigned long long>(rng()));
  return buffer;
}

std::filesystem::path AppDataDir() {
#ifdef _WIN32
  wchar_t* raw = nullptr;
  std::filesystem::path base;
  if (SUCCEEDED(
        SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw))) {
    base = raw;
  }
  CoTaskMemFree(raw);
  auto dir = base / L"FlightSimHub";
  std::filesystem::create_directories(dir);
  return dir;
#else
  return std::filesystem::temp_directory_path() / "FlightSimHub";
#endif
}

}  // namespace FSHub
