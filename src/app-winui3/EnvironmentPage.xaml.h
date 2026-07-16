#pragma once

#include "EnvironmentPage.xaml.g.h"

#include <string>

namespace winrt::FlightSimHubApp::implementation {

struct EnvironmentPage : EnvironmentPageT<EnvironmentPage> {
  EnvironmentPage();

  // Detection probes the filesystem and registry; keep them off the UI thread
  fire_and_forget OnRescanClick(
    Windows::Foundation::IInspectable const&,
    Microsoft::UI::Xaml::RoutedEventArgs const&);

 private:
  void RebuildList();
  Microsoft::UI::Xaml::UIElement BuildRow(const std::string& appId);
  fire_and_forget InstallClicked(std::string appId);
  void LocateClicked(const std::string& appId);
  void ClearOverrideClicked(const std::string& appId);
};

}  // namespace winrt::FlightSimHubApp::implementation

namespace winrt::FlightSimHubApp::factory_implementation {

struct EnvironmentPage
  : EnvironmentPageT<EnvironmentPage, implementation::EnvironmentPage> {};

}  // namespace winrt::FlightSimHubApp::factory_implementation
