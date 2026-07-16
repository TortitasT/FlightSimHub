#include "pch.h"

#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "EnvironmentPage.xaml.h"
#include "LaunchersPage.xaml.h"

#include <microsoft.ui.xaml.window.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::FlightSimHubApp::implementation {

MainWindow::MainWindow() {
  InitializeComponent();
  Title(L"FlightSimHub");

  // The XAML window has no icon by default; point it at the exe's
  // embedded "appIcon" resource, same pattern as FreeOpenKneeboard
  HWND hwnd {};
  check_hresult(get_strong().as<IWindowNative>()->get_WindowHandle(&hwnd));
  const auto setIcon = [&](WPARAM type, int metric) {
    const auto icon = LoadImageW(
      GetModuleHandleW(nullptr),
      L"appIcon",
      IMAGE_ICON,
      GetSystemMetrics(metric),
      GetSystemMetrics(metric),
      0);
    SendMessageW(hwnd, WM_SETICON, type, reinterpret_cast<LPARAM>(icon));
  };
  setIcon(ICON_BIG, SM_CXICON);
  setIcon(ICON_SMALL, SM_CXSMICON);

  Navigation().SelectedItem(EnvironmentItem());
}

void MainWindow::OnNavigationChanged(
  NavigationView const&,
  NavigationViewSelectionChangedEventArgs const& args) {
  const auto item = args.SelectedItem().try_as<NavigationViewItem>();
  if (!item) {
    return;
  }
  const auto tag = unbox_value_or<hstring>(item.Tag(), L"");
  if (tag == L"environment") {
    ContentFrame().Navigate(xaml_typename<FlightSimHubApp::EnvironmentPage>());
  } else if (tag == L"launchers") {
    ContentFrame().Navigate(xaml_typename<FlightSimHubApp::LaunchersPage>());
  }
}

}  // namespace winrt::FlightSimHubApp::implementation
