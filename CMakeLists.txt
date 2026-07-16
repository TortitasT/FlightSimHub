cmake_minimum_required(VERSION 3.25..4.0 FATAL_ERROR)

# Require that link targets exist
cmake_policy(SET CMP0079 NEW)
# Enable CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0091 NEW)

# Win10 19H1 baseline, same as FreeOpenKneeboard
set(NTDDI_VERSION "NTDDI_WIN10_19H1")
set(WIN32_WINNT "_WIN32_WINNT_WIN10")
set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.20348.0" CACHE INTERNAL "Minimum windows version" FORCE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(
  CMAKE_VS_GLOBALS
  "AppxPackage=false"
  "RuntimeIdentifier=win10-x64"
  "RuntimeIdentifiers=win10-x64"
  "BaseNuGetRuntimeIdentifier=win10"
  "AssetTargetFallback=$(AssetTargetFallback);native"
)

add_compile_options("/EHsc")

# Hybrid CRT: static C++ runtime + dynamic UCRT
# https://github.com/microsoft/WindowsAppSDK/blob/main/docs/Coding-Guidelines/HybridCRT.md
set(
  CMAKE_MSVC_RUNTIME_LIBRARY
  "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
add_link_options(
  "/DEFAULTLIB:ucrt$<$<CONFIG:Debug>:d>.lib"
  "/NODEFAULTLIB:libucrt$<$<CONFIG:Debug>:d>.lib"
)

set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo)

project(FlightSimHub VERSION 0.1.0 LANGUAGES CXX)

string(REGEX MATCH "^Visual Studio" GENERATOR_IS_VS "${CMAKE_GENERATOR}")
if (NOT GENERATOR_IS_VS)
  message(FATAL_ERROR "The CMake generator '${CMAKE_GENERATOR}' is not supported; use the Visual Studio generator instead.")
endif ()

add_compile_options(
  "/D_WIN32_WINNT=${WIN32_WINNT}"
  "/DNTDDI_VERSION=${NTDDI_VERSION}"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(windowsappsdk)

# Don't implicitly copy .dlls from vcpkg to runtime dirs
set(VCPKG_APPLOCAL_DEPS OFF)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Directory.Build.props"
  "${CMAKE_CURRENT_BINARY_DIR}/Directory.Build.props"
  COPYONLY
)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT FSHub-App)
