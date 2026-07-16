#ifdef _WIN32

#include <FSHub/Strings.hpp>
#include <FSHub/WindowsProbe.hpp>

#include <windows.h>

#include <fstream>
#include <sstream>

namespace FSHub {

std::optional<std::string> WindowsProbe::ReadRegistryString(
  const std::string& root,
  const std::string& path,
  const std::string& value) const {
  HKEY rootKey = nullptr;
  if (root == "HKCU") {
    rootKey = HKEY_CURRENT_USER;
  } else if (root == "HKLM") {
    rootKey = HKEY_LOCAL_MACHINE;
  } else {
    return std::nullopt;
  }

  const auto widePath = Widen(path);
  const auto wideValue = Widen(value);
  DWORD size = 0;
  if (
    RegGetValueW(
      rootKey,
      widePath.c_str(),
      wideValue.c_str(),
      // installers commonly write InstallLocation as REG_EXPAND_SZ
      RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
      nullptr,
      nullptr,
      &size)
    != ERROR_SUCCESS) {
    return std::nullopt;
  }
  std::wstring buffer(size / sizeof(wchar_t), L'\0');
  if (
    RegGetValueW(
      rootKey,
      widePath.c_str(),
      wideValue.c_str(),
      // installers commonly write InstallLocation as REG_EXPAND_SZ
      RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
      nullptr,
      buffer.data(),
      &size)
    != ERROR_SUCCESS) {
    return std::nullopt;
  }
  buffer.resize(wcsnlen(buffer.data(), buffer.size()));
  return Narrow(buffer);
}

bool WindowsProbe::FileExists(const std::filesystem::path& path) const {
  std::error_code ec;
  return std::filesystem::exists(path, ec);
}

std::string WindowsProbe::ExpandEnvironmentVars(
  const std::string& input) const {
  const auto wide = Widen(input);
  const DWORD size
    = ExpandEnvironmentStringsW(wide.c_str(), nullptr, 0);
  if (size == 0) {
    return input;
  }
  std::wstring buffer(size, L'\0');
  ExpandEnvironmentStringsW(wide.c_str(), buffer.data(), size);
  buffer.resize(wcsnlen(buffer.data(), buffer.size()));
  return Narrow(buffer);
}

std::optional<std::string> WindowsProbe::ReadTextFile(
  const std::filesystem::path& path) const {
  std::ifstream stream(path);
  if (!stream.good()) {
    return std::nullopt;
  }
  std::ostringstream content;
  content << stream.rdbuf();
  return content.str();
}

std::vector<std::filesystem::path> WindowsProbe::ListSubdirectories(
  const std::filesystem::path& path) const {
  std::vector<std::filesystem::path> result;
  std::error_code ec;
  for (const auto& entry: std::filesystem::directory_iterator(path, ec)) {
    if (entry.is_directory(ec)) {
      result.push_back(entry.path());
    }
  }
  return result;
}

}  // namespace FSHub

#endif  // _WIN32
