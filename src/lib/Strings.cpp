#ifdef _WIN32

#include <FSHub/Strings.hpp>

#include <windows.h>

namespace FSHub {

std::wstring Widen(const std::string& utf8) {
  if (utf8.empty()) {
    return {};
  }
  const int size = MultiByteToWideChar(
    CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring result(size, L'\0');
  MultiByteToWideChar(
    CP_UTF8,
    0,
    utf8.data(),
    static_cast<int>(utf8.size()),
    result.data(),
    size);
  return result;
}

std::string Narrow(const std::wstring& utf16) {
  if (utf16.empty()) {
    return {};
  }
  const int size = WideCharToMultiByte(
    CP_UTF8,
    0,
    utf16.data(),
    static_cast<int>(utf16.size()),
    nullptr,
    0,
    nullptr,
    nullptr);
  std::string result(size, '\0');
  WideCharToMultiByte(
    CP_UTF8,
    0,
    utf16.data(),
    static_cast<int>(utf16.size()),
    result.data(),
    size,
    nullptr,
    nullptr);
  return result;
}

}  // namespace FSHub

#endif  // _WIN32
