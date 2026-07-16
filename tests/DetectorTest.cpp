#include <FSHub/Detector.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>

using namespace FSHub;

namespace {

class FakeProbe final : public IProbe {
 public:
  // key: "root|path|value"
  std::map<std::string, std::string> registry;
  std::set<std::filesystem::path> files;
  std::map<std::string, std::string> env;
  std::map<std::filesystem::path, std::string> textFiles;

  std::optional<std::string> ReadRegistryString(
    const std::string& root,
    const std::string& path,
    const std::string& value) const override {
    const auto it = registry.find(root + "|" + path + "|" + value);
    if (it == registry.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  bool FileExists(const std::filesystem::path& path) const override {
    return files.contains(path);
  }

  std::string ExpandEnvironmentVars(const std::string& input) const override {
    auto result = input;
    for (const auto& [name, value]: env) {
      const auto token = "%" + name + "%";
      if (const auto pos = result.find(token); pos != std::string::npos) {
        result.replace(pos, token.size(), value);
      }
    }
    return result;
  }

  std::optional<std::string> ReadTextFile(
    const std::filesystem::path& path) const override {
    const auto it = textFiles.find(path);
    if (it == textFiles.end()) {
      return std::nullopt;
    }
    return it->second;
  }
};

AppDefinition BmsApp() {
  return AppDefinition {
    .id = "falcon-bms",
    .name = "Falcon BMS",
    .exeName = "Falcon BMS.exe",
    .kind = AppKind::Sim,
    .detection = Detection {
      .registryKeys = {{
        .root = "HKLM",
        .path = "SOFTWARE\\WOW6432Node\\Benchmark Sims\\Falcon BMS 4.37",
        .value = "baseDir",
        .append = "Bin/x64/Falcon BMS.exe",
      }},
      .wellKnownPaths = {"%ProgramFiles%/Falcon BMS 4.37/Bin/x64/Falcon BMS.exe"},
      .steamRelativeExe = "Falcon BMS 4.37/Bin/x64/Falcon BMS.exe",
    },
  };
}

AppDefinition PortableApp() {
  return AppDefinition {
    .id = "aitrack",
    .name = "AITrack",
    .exeName = "AITrack.exe",
    .kind = AppKind::Companion,
  };
}

constexpr auto kManagedDir = "C:/Users/me/AppData/Local/FlightSimHub/apps";

const std::string kVdf = R"("libraryfolders"
{
  "0"
  {
    "path"    "C:\\Program Files (x86)\\Steam"
    "label"    ""
  }
  "1"
  {
    "path"    "D:\\SteamLibrary"
  }
}
)";

}  // namespace

TEST_CASE("ParseSteamLibraryFolders extracts library paths", "[Detector]") {
  const auto paths = ParseSteamLibraryFolders(kVdf);
  REQUIRE(paths.size() == 2);
  CHECK(paths.at(0) == std::filesystem::path("C:/Program Files (x86)/Steam"));
  CHECK(paths.at(1) == std::filesystem::path("D:/SteamLibrary"));
}

TEST_CASE("Existing override wins as LocatedManually", "[Detector]") {
  FakeProbe probe;
  probe.files.insert("C:/custom/Falcon BMS.exe");
  Detector detector(probe, kManagedDir);

  const auto state
    = detector.Resolve(BmsApp(), std::filesystem::path("C:/custom/Falcon BMS.exe"));
  CHECK(state.status == InstallState::Status::LocatedManually);
  CHECK(state.exePath == std::filesystem::path("C:/custom/Falcon BMS.exe"));
}

TEST_CASE("Override with missing file falls through to detection", "[Detector]") {
  FakeProbe probe;
  probe.registry["HKLM|SOFTWARE\\WOW6432Node\\Benchmark Sims\\Falcon BMS 4.37|baseDir"]
    = "C:/Falcon BMS 4.37";
  probe.files.insert("C:/Falcon BMS 4.37/Bin/x64/Falcon BMS.exe");
  Detector detector(probe, kManagedDir);

  const auto state
    = detector.Resolve(BmsApp(), std::filesystem::path("C:/gone/Falcon BMS.exe"));
  CHECK(state.status == InstallState::Status::Detected);
  CHECK(
    state.exePath
    == std::filesystem::path("C:/Falcon BMS 4.37/Bin/x64/Falcon BMS.exe"));
}

TEST_CASE("Registry hint joins append path", "[Detector]") {
  FakeProbe probe;
  probe.registry["HKLM|SOFTWARE\\WOW6432Node\\Benchmark Sims\\Falcon BMS 4.37|baseDir"]
    = "C:/Falcon BMS 4.37";
  probe.files.insert("C:/Falcon BMS 4.37/Bin/x64/Falcon BMS.exe");
  Detector detector(probe, kManagedDir);

  const auto state = detector.Resolve(BmsApp(), std::nullopt);
  CHECK(state.status == InstallState::Status::Detected);
  CHECK(
    state.exePath
    == std::filesystem::path("C:/Falcon BMS 4.37/Bin/x64/Falcon BMS.exe"));
}

TEST_CASE("Registry hit with missing exe falls through to well-known path", "[Detector]") {
  FakeProbe probe;
  probe.registry["HKLM|SOFTWARE\\WOW6432Node\\Benchmark Sims\\Falcon BMS 4.37|baseDir"]
    = "C:/Falcon BMS 4.37";
  probe.env["ProgramFiles"] = "C:/Program Files";
  probe.files.insert("C:/Program Files/Falcon BMS 4.37/Bin/x64/Falcon BMS.exe");
  Detector detector(probe, kManagedDir);

  const auto state = detector.Resolve(BmsApp(), std::nullopt);
  CHECK(state.status == InstallState::Status::Detected);
  CHECK(
    state.exePath
    == std::filesystem::path(
      "C:/Program Files/Falcon BMS 4.37/Bin/x64/Falcon BMS.exe"));
}

TEST_CASE("Steam library scan finds the app", "[Detector]") {
  FakeProbe probe;
  probe.registry["HKCU|Software\\Valve\\Steam|SteamPath"]
    = "C:/Program Files (x86)/Steam";
  probe.textFiles
    [std::filesystem::path(
      "C:/Program Files (x86)/Steam/steamapps/libraryfolders.vdf")]
    = kVdf;
  probe.files.insert(
    "D:/SteamLibrary/steamapps/common/Falcon BMS 4.37/Bin/x64/Falcon BMS.exe");
  Detector detector(probe, kManagedDir);

  const auto state = detector.Resolve(BmsApp(), std::nullopt);
  CHECK(state.status == InstallState::Status::Detected);
  CHECK(
    state.exePath
    == std::filesystem::path(
      "D:/SteamLibrary/steamapps/common/Falcon BMS 4.37/Bin/x64/Falcon BMS.exe"));
}

TEST_CASE("Managed apps dir is checked for portable apps", "[Detector]") {
  FakeProbe probe;
  probe.files.insert(
    std::filesystem::path(kManagedDir) / "aitrack" / "AITrack.exe");
  Detector detector(probe, kManagedDir);

  const auto state = detector.Resolve(PortableApp(), std::nullopt);
  CHECK(state.status == InstallState::Status::Detected);
  CHECK(
    state.exePath
    == std::filesystem::path(kManagedDir) / "aitrack" / "AITrack.exe");
}

TEST_CASE("Nothing found anywhere is NotFound", "[Detector]") {
  FakeProbe probe;
  Detector detector(probe, kManagedDir);

  const auto state = detector.Resolve(BmsApp(), std::nullopt);
  CHECK(state.status == InstallState::Status::NotFound);
  CHECK(state.exePath.empty());
}
