#pragma once

#include <string>

namespace FSHub {

// UTF-8 <-> UTF-16. All narrow strings in FSHub (settings JSON, catalog,
// UI text via winrt::to_string) are UTF-8; never widen them byte-by-byte.
std::wstring Widen(const std::string& utf8);
std::string Narrow(const std::wstring& utf16);

}  // namespace FSHub
