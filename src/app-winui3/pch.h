#pragma once

#define NOMINMAX 1

// clang-format off
#include <unknwn.h>
#include <windows.h>
#include <hstring.h>
#include <restrictederrorinfo.h>
#include <winrt/base.h>
// clang-format on

// Storyboard::GetCurrentTime conflicts with the windows.h macro
#undef GetCurrentTime

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.System.h>

#include <wil/cppwinrt.h>
#include <wil/cppwinrt_helpers.h>
