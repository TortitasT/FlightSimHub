#ifdef _WIN32

#include <FSHub/ShortcutWriter.hpp>

#include <windows.h>

#include <shlobj.h>
#include <shobjidl.h>

#include <wil/com.h>
#include <wil/resource.h>

#include <filesystem>

namespace FSHub {

namespace {

std::wstring SanitizeFileName(const std::string& name) {
  std::wstring result;
  for (const char c: name) {
    if (std::string_view {R"(\/:*?"<>|)"}.contains(c)) {
      result.push_back(L'_');
    } else {
      result.push_back(static_cast<wchar_t>(c));
    }
  }
  return result;
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

  wchar_t selfPath[MAX_PATH] {};
  if (!GetModuleFileNameW(nullptr, selfPath, MAX_PATH)) {
    return std::unexpected("cannot resolve the FlightSimHub executable path");
  }

  try {
    auto link = wil::CoCreateInstance<IShellLinkW>(CLSID_ShellLink);
    link->SetPath(selfPath);
    const auto args = L"--launch "
      + std::wstring {launcherId.begin(), launcherId.end()};
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
