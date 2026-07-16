#include <FSHub/Settings.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

using namespace FSHub;

namespace {

Settings SampleSettings() {
  return Settings {
    .appOverrides = {{"wdp", "C:/Tools/WDP/WDP.exe"}},
    .launchers = {Launcher {
      .id = "11111111-2222-3333-4444-555555555555",
      .name = "BMS Night Ops",
      .items = {
        {.appId = "opentrack", .args = "", .delayAfterSeconds = 3},
        {.appId = "falcon-bms", .args = "-window", .delayAfterSeconds = 0},
      },
      .closeCompanionsOnSimExit = false,
    }},
  };
}

struct TempDir {
  std::filesystem::path path;
  TempDir() {
    // Unique per instance: ctest -j runs these test cases concurrently
    path = std::filesystem::temp_directory_path()
      / ("fshub-settings-test-" + NewLauncherId());
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
  }
  ~TempDir() {
    std::filesystem::remove_all(path);
  }
};

}  // namespace

TEST_CASE("Settings json round-trip", "[Settings]") {
  const auto original = SampleSettings();
  const nlohmann::json json = original;
  const auto restored = json.get<Settings>();

  CHECK(restored.appOverrides == original.appOverrides);
  REQUIRE(restored.launchers.size() == 1);
  const auto& launcher = restored.launchers.at(0);
  CHECK(launcher.id == original.launchers.at(0).id);
  CHECK(launcher.name == "BMS Night Ops");
  CHECK(launcher.closeCompanionsOnSimExit == false);
  REQUIRE(launcher.items.size() == 2);
  CHECK(launcher.items.at(0).appId == "opentrack");
  CHECK(launcher.items.at(0).delayAfterSeconds == 3);
  CHECK(launcher.items.at(1).args == "-window");
  CHECK(launcher.items.at(1).delayAfterSeconds == 0);
}

TEST_CASE("Settings missing fields use defaults", "[Settings]") {
  const auto json = nlohmann::json::parse(R"({
    "launchers": [
      {"id": "x", "name": "y", "items": [{"appId": "dcs"}]}
    ]
  })");
  const auto settings = json.get<Settings>();
  CHECK(settings.appOverrides.empty());
  REQUIRE(settings.launchers.size() == 1);
  CHECK(settings.launchers.at(0).closeCompanionsOnSimExit == true);
  CHECK(settings.launchers.at(0).items.at(0).delayAfterSeconds == 3);
  CHECK(settings.launchers.at(0).items.at(0).args.empty());
}

TEST_CASE("LoadSettings returns defaults for missing file", "[Settings]") {
  TempDir dir;
  const auto result = LoadSettings(dir.path / "settings.json");
  CHECK_FALSE(result.wasCorrupt);
  CHECK(result.settings.launchers.empty());
  CHECK(result.settings.appOverrides.empty());
}

TEST_CASE("LoadSettings renames corrupt file and returns defaults", "[Settings]") {
  TempDir dir;
  const auto file = dir.path / "settings.json";
  std::ofstream(file) << "{not json";

  const auto result = LoadSettings(file);
  CHECK(result.wasCorrupt);
  CHECK(result.settings.launchers.empty());
  CHECK_FALSE(std::filesystem::exists(file));
  CHECK(std::filesystem::exists(dir.path / "settings.json.corrupt"));
}

TEST_CASE("SaveSettings then LoadSettings round-trips", "[Settings]") {
  TempDir dir;
  const auto file = dir.path / "settings.json";
  SaveSettings(file, SampleSettings());

  const auto result = LoadSettings(file);
  CHECK_FALSE(result.wasCorrupt);
  REQUIRE(result.settings.launchers.size() == 1);
  CHECK(result.settings.launchers.at(0).name == "BMS Night Ops");
  CHECK(result.settings.appOverrides.at("wdp") == "C:/Tools/WDP/WDP.exe");
}

TEST_CASE("NewLauncherId returns unique non-empty ids", "[Settings]") {
  const auto a = NewLauncherId();
  const auto b = NewLauncherId();
  CHECK_FALSE(a.empty());
  CHECK(a != b);
}
