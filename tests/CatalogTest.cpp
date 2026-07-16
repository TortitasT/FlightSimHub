#include <FSHub/AppDefinition.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>

#include <fstream>

using namespace FSHub;

namespace {

nlohmann::json MinimalCatalog() {
  return nlohmann::json::parse(R"([
    {
      "id": "falcon-bms",
      "name": "Falcon BMS",
      "kind": "sim",
      "exeName": "Falcon BMS.exe",
      "detection": {
        "registryKeys": [
          {
            "root": "HKLM",
            "path": "SOFTWARE\\WOW6432Node\\Benchmark Sims\\Falcon BMS 4.37",
            "value": "baseDir",
            "append": "Bin/x64/Falcon BMS.exe"
          }
        ],
        "steamRelativeExe": "Falcon BMS 4.37/Bin/x64/Falcon BMS.exe"
      },
      "source": {
        "type": "manual",
        "homepage": "https://www.falcon-bms.com/downloads/"
      }
    },
    {
      "id": "opentrack",
      "name": "OpenTrack",
      "kind": "companion",
      "exeName": "opentrack.exe",
      "detection": {
        "wellKnownPaths": ["%ProgramFiles(x86)%/opentrack/opentrack.exe"]
      },
      "source": {
        "type": "github",
        "repo": "opentrack/opentrack",
        "assetPattern": "(?i).*install.*\\.exe$",
        "installKind": "installer"
      }
    }
  ])");
}

}  // namespace

TEST_CASE("ParseCatalog parses every field", "[Catalog]") {
  const auto apps = ParseCatalog(MinimalCatalog());
  REQUIRE(apps.size() == 2);

  const auto& bms = apps.at(0);
  CHECK(bms.id == "falcon-bms");
  CHECK(bms.name == "Falcon BMS");
  CHECK(bms.kind == AppKind::Sim);
  CHECK(bms.exeName == "Falcon BMS.exe");
  REQUIRE(bms.detection.registryKeys.size() == 1);
  CHECK(bms.detection.registryKeys.at(0).root == "HKLM");
  CHECK(
    bms.detection.registryKeys.at(0).path
    == "SOFTWARE\\WOW6432Node\\Benchmark Sims\\Falcon BMS 4.37");
  CHECK(bms.detection.registryKeys.at(0).value == "baseDir");
  CHECK(bms.detection.registryKeys.at(0).append == "Bin/x64/Falcon BMS.exe");
  CHECK(bms.detection.wellKnownPaths.empty());
  REQUIRE(bms.detection.steamRelativeExe.has_value());
  CHECK(*bms.detection.steamRelativeExe == "Falcon BMS 4.37/Bin/x64/Falcon BMS.exe");
  CHECK(bms.source.type == SourceType::Manual);
  CHECK(bms.source.homepage == "https://www.falcon-bms.com/downloads/");
  CHECK(bms.source.installKind == InstallKind::None);

  const auto& ot = apps.at(1);
  CHECK(ot.kind == AppKind::Companion);
  CHECK(ot.detection.registryKeys.empty());
  CHECK_FALSE(ot.detection.steamRelativeExe.has_value());
  REQUIRE(ot.detection.wellKnownPaths.size() == 1);
  CHECK(ot.detection.wellKnownPaths.at(0) == "%ProgramFiles(x86)%/opentrack/opentrack.exe");
  CHECK(ot.source.type == SourceType::GitHub);
  CHECK(ot.source.repo == "opentrack/opentrack");
  CHECK(ot.source.assetPattern == "(?i).*install.*\\.exe$");
  CHECK(ot.source.installKind == InstallKind::Installer);
}

TEST_CASE("ParseCatalog rejects entry without id", "[Catalog]") {
  auto json = MinimalCatalog();
  json.at(0).erase("id");
  CHECK_THROWS_WITH(ParseCatalog(json), Catch::Matchers::ContainsSubstring("id"));
}

TEST_CASE("ParseCatalog rejects unknown kind", "[Catalog]") {
  auto json = MinimalCatalog();
  json.at(0)["kind"] = "helicopter";
  CHECK_THROWS_WITH(ParseCatalog(json), Catch::Matchers::ContainsSubstring("falcon-bms"));
}

TEST_CASE("ParseCatalog rejects github source without repo", "[Catalog]") {
  auto json = MinimalCatalog();
  json.at(1)["source"].erase("repo");
  CHECK_THROWS_WITH(ParseCatalog(json), Catch::Matchers::ContainsSubstring("opentrack"));
}

TEST_CASE("Shipped catalog parses with expected entries", "[Catalog]") {
  std::ifstream file(FSHUB_TEST_DATA_DIR "/catalog.json");
  REQUIRE(file.good());
  const auto apps = ParseCatalog(nlohmann::json::parse(file));

  REQUIRE(apps.size() == 7);
  const std::vector<std::string> expectedIds {
    "dcs",
    "falcon-bms",
    "freeopenkneeboard",
    "aitrack",
    "opentrack",
    "wdp",
    "ezboards",
  };
  for (size_t i = 0; i < expectedIds.size(); ++i) {
    CHECK(apps.at(i).id == expectedIds.at(i));
  }

  size_t simCount = 0;
  for (const auto& app: apps) {
    if (app.kind == AppKind::Sim) {
      ++simCount;
    }
    if (app.id == "freeopenkneeboard" || app.id == "aitrack" || app.id == "opentrack") {
      CHECK(app.source.type == SourceType::GitHub);
      CHECK_FALSE(app.source.repo.empty());
      CHECK_FALSE(app.source.assetPattern.empty());
    } else {
      CHECK(app.source.type == SourceType::Manual);
      CHECK_FALSE(app.source.homepage.empty());
    }
    if (app.id == "aitrack") {
      CHECK(app.source.installKind == InstallKind::Portable);
    }
  }
  CHECK(simCount == 2);
}
