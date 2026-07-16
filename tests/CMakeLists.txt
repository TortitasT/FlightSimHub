find_package(Catch2 CONFIG REQUIRED)

add_executable(
  FSHub-Tests
  CatalogTest.cpp
  DetectorTest.cpp
  InstallerTest.cpp
  LaunchPlanTest.cpp
  SettingsTest.cpp
)

target_link_libraries(
  FSHub-Tests
  PRIVATE
  FSHub-Lib
  Catch2::Catch2WithMain
)

target_compile_definitions(
  FSHub-Tests
  PRIVATE
  "FSHUB_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/data\""
)

include(Catch)
catch_discover_tests(FSHub-Tests)
