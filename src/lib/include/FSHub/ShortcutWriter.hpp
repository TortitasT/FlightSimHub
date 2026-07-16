#pragma once

#include <expected>
#include <string>

namespace FSHub {

// Creates %APPDATA%\Microsoft\Windows\Start Menu\Programs\FlightSimHub\
// <launcherName>.lnk targeting the current exe with "--launch <launcherId>".
// Windows-only.
std::expected<void, std::string> CreateStartMenuShortcut(
  const std::string& launcherName, const std::string& launcherId);

bool StartMenuShortcutExists(const std::string& launcherName);

std::expected<void, std::string> RemoveStartMenuShortcut(
  const std::string& launcherName);

}  // namespace FSHub
