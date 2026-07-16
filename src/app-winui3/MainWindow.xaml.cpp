#include "pch.h"

#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "EnvironmentPage.xaml.h"
#include "LaunchersPage.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::FlightSimHubApp::implementation {

MainWindow::MainWindow() {
  InitializeComponent();
  Title(L"FlightSimHub");
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
