find_package(nlohmann_json CONFIG REQUIRED)
find_package(cppwinrt CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)

add_library(
  FSHub-Lib
  STATIC
  AppDefinition.cpp
  Detector.cpp
  Installer.cpp
  LauncherEngine.cpp
  Settings.cpp
  ShortcutWriter.cpp
  Strings.cpp
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
  Microsoft::CppWinRT
  WIL::WIL
  WindowsApp
  Shell32
  Ole32
  OleAut32
  User32
)
