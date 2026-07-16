find_package(nlohmann_json CONFIG REQUIRED)

add_library(
  FSHub-Lib
  STATIC
  AppDefinition.cpp
  Detector.cpp
  Installer.cpp
  LauncherEngine.cpp
  Settings.cpp
  ShortcutWriter.cpp
  WindowsProbe.cpp
)

target_include_directories(
  FSHub-Lib
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(
  FSHub-Lib
  PUBLIC
  nlohmann_json::nlohmann_json
)
