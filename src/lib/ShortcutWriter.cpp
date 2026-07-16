#ifdef _WIN32

#include <FSHub/ShortcutWriter.hpp>
#include <FSHub/Strings.hpp>

#include <windows.h>

#include <shlobj.h>
#include <shobjidl.h>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>

#include <filesystem>
#include <string_view>

namespace FSHub {

namespace {

std::wstring SanitizeFileName(const std::string& name) {
  auto wide = Widen(name);
  for (auto& c: wide) {
    if (std::wstring_view {LR"(\/:*?"<>|)"}.contains(c)) {
      c = L'_';
    }
  }
  return wide;
}

std::expected<std::filesystem::path, std::string> ShortcutPath(
  const std::string& launcherName) {
  wil::unique_cotaskmem_string programs;
  if (FAILED(SHGetKnownFolderPath(
        FOLDERID_Programs, KF_FLAG_CREATE, nullptr, &programs))) {
    return std::unexpected("cannot resolve the Start Menu Programs folder");
  }
  const auto dir = std::filesystem::path {programs.get()} / L"FlightSimHub";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return std::unexpected("cannot create the FlightSimHub Start Menu folder");
  }
  return dir / (SanitizeFileName(launcherName) + L".lnk");
}

}  // namespace

std::expected<void, std::string> CreateStartMenuShortcut(
  const std::string& launcherName, const std::string& launcherId) {
  const auto lnkPath = ShortcutPath(launcherName);
  if (!lnkPath) {
    return std::unexpected(lnkPath.error());
  }

  try {
    const auto selfPath = wil::GetModuleFileNameW<std::wstring>(nullptr);

    auto link = wil::CoCreateInstance<IShellLinkW>(CLSID_ShellLink);
    link->SetPath(selfPath.c_str());
    const auto args = L"--launch " + Widen(launcherId);
    link->SetArguments(args.c_str());
    link->SetDescription(L"FlightSimHub launcher");

    auto persist = link.query<IPersistFile>();
    if (FAILED(persist->Save(lnkPath->c_str(), TRUE))) {
      return std::unexpected("failed to save the shortcut");
    }
  } catch (const wil::ResultException& e) {
    return std::unexpected(e.what());
  }
  return {};
}

bool StartMenuShortcutExists(const std::string& launcherName) {
  const auto lnkPath = ShortcutPath(launcherName);
  if (!lnkPath) {
    return false;
  }
  std::error_code ec;
  return std::filesystem::exists(*lnkPath, ec);
}

std::expected<void, std::string> RemoveStartMenuShortcut(
  const std::string& launcherName) {
  const auto lnkPath = ShortcutPath(launcherName);
  if (!lnkPath) {
    return std::unexpected(lnkPath.error());
  }
  std::error_code ec;
  std::filesystem::remove(*lnkPath, ec);
  if (ec) {
    return std::unexpected("failed to remove the shortcut");
  }
  return {};
}

}  // namespace FSHub

#endif  // _WIN32
