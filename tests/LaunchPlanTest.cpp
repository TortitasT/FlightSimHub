#include <FSHub/LauncherEngine.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace FSHub;

namespace {

std::vector<AppDefinition> Catalog() {
  return {
    {.id = "falcon-bms", .name = "Falcon BMS", .exeName = "Falcon BMS.exe", .kind = AppKind::Sim},
    {.id = "dcs", .name = "DCS World", .exeName = "DCS.exe", .kind = AppKind::Sim},
    {.id = "opentrack", .name = "OpenTrack", .exeName = "opentrack.exe", .kind = AppKind::Companion},
  };
}

std::map<std::string, InstallState> AllFound() {
  return {
    {"falcon-bms",
     {InstallState::Status::Detected, "C:/BMS/Bin/x64/Falcon BMS.exe"}},
    {"dcs", {InstallState::Status::Detected, "C:/DCS/bin/DCS.exe"}},
    {"opentrack",
     {InstallState::Status::LocatedManually, "C:/ot/opentrack.exe"}},
  };
}

Launcher ValidLauncher() {
  return Launcher {
    .id = "l1",
    .name = "BMS",
    .items = {
      {.appId = "opentrack", .args = "--profile x", .delayAfterSeconds = 5},
      {.appId = "falcon-bms", .args = "", .delayAfterSeconds = 0},
    },
  };
}

}  // namespace

TEST_CASE("BuildLaunchPlan resolves paths, args and sim flag", "[LaunchPlan]") {
  const auto plan = BuildLaunchPlan(ValidLauncher(), Catalog(), AllFound());
  REQUIRE(plan.has_value());
  REQUIRE(plan->size() == 2);

  CHECK(plan->at(0).appId == "opentrack");
  CHECK(plan->at(0).exe == std::filesystem::path("C:/ot/opentrack.exe"));
  CHECK(plan->at(0).args == "--profile x");
  CHECK(plan->at(0).delayAfterSeconds == 5);
  CHECK_FALSE(plan->at(0).isSim);

  CHECK(plan->at(1).appId == "falcon-bms");
  CHECK(plan->at(1).isSim);
}

TEST_CASE("BuildLaunchPlan rejects unknown app id", "[LaunchPlan]") {
  auto launcher = ValidLauncher();
  launcher.items.at(0).appId = "nonexistent";
  const auto plan = BuildLaunchPlan(launcher, Catalog(), AllFound());
  REQUIRE_FALSE(plan.has_value());
  CHECK_THAT(plan.error(), Catch::Matchers::ContainsSubstring("nonexistent"));
}

TEST_CASE("BuildLaunchPlan rejects apps that are not found", "[LaunchPlan]") {
  auto states = AllFound();
  states["opentrack"] = {};
  const auto plan = BuildLaunchPlan(ValidLauncher(), Catalog(), states);
  REQUIRE_FALSE(plan.has_value());
  CHECK_THAT(plan.error(), Catch::Matchers::ContainsSubstring("OpenTrack"));
}

TEST_CASE("BuildLaunchPlan requires exactly one sim", "[LaunchPlan]") {
  auto noSim = ValidLauncher();
  noSim.items.pop_back();
  REQUIRE_FALSE(BuildLaunchPlan(noSim, Catalog(), AllFound()).has_value());

  auto twoSims = ValidLauncher();
  twoSims.items.push_back({.appId = "dcs"});
  REQUIRE_FALSE(BuildLaunchPlan(twoSims, Catalog(), AllFound()).has_value());
}

TEST_CASE("BuildLaunchPlan rejects empty launchers", "[LaunchPlan]") {
  Launcher empty {.id = "e", .name = "Empty"};
  REQUIRE_FALSE(BuildLaunchPlan(empty, Catalog(), AllFound()).has_value());
}
