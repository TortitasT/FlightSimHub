#include <FSHub/Installer.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace FSHub;

namespace {

nlohmann::json SampleRelease() {
  return nlohmann::json::parse(R"({
    "tag_name": "v1.2.0",
    "assets": [
      {"name": "SourceCode.tar.gz", "browser_download_url": "https://example.com/src.tar.gz"},
      {"name": "OpenKneeboard-v1.2.msi", "browser_download_url": "https://example.com/okb.msi"},
      {"name": "OpenKneeboard-v1.2-Symbols.zip", "browser_download_url": "https://example.com/sym.zip"}
    ]
  })");
}

}  // namespace

TEST_CASE("SelectAsset picks the first matching asset", "[Installer]") {
  const auto asset = SelectAsset(SampleRelease(), R"(.*\.(msi|exe)$)");
  REQUIRE(asset.has_value());
  CHECK(asset->name == "OpenKneeboard-v1.2.msi");
  CHECK(asset->downloadUrl == "https://example.com/okb.msi");
}

TEST_CASE("SelectAsset honors the (?i) case-insensitive prefix", "[Installer]") {
  const auto asset = SelectAsset(SampleRelease(), R"((?i)openkneeboard.*\.MSI$)");
  REQUIRE(asset.has_value());
  CHECK(asset->name == "OpenKneeboard-v1.2.msi");
}

TEST_CASE("SelectAsset returns nullopt when nothing matches", "[Installer]") {
  CHECK_FALSE(SelectAsset(SampleRelease(), R"(.*\.dmg$)").has_value());
}

TEST_CASE("SelectAsset returns nullopt when assets are missing", "[Installer]") {
  CHECK_FALSE(
    SelectAsset(nlohmann::json::parse(R"({"tag_name": "v1"})"), ".*")
      .has_value());
}
