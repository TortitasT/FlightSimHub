#pragma once

#include "MainWindow.xaml.g.h"

namespace winrt::FlightSimHubApp::implementation {

struct MainWindow : MainWindowT<MainWindow> {
  MainWindow();

  void OnNavigationChanged(
    Microsoft::UI::Xaml::Controls::NavigationView const&,
    Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs
      const&);
};

}  // namespace winrt::FlightSimHubApp::implementation

namespace winrt::FlightSimHubApp::factory_implementation {

struct MainWindow
  : MainWindowT<MainWindow, implementation::MainWindow> {};

}  // namespace winrt::FlightSimHubApp::factory_implementation
