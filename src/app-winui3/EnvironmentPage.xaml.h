#pragma once

#include "EnvironmentPage.xaml.g.h"

#include <string>

namespace winrt::FlightSimHubApp::implementation {

struct EnvironmentPage : EnvironmentPageT<EnvironmentPage> {
  EnvironmentPage();

  void OnRescanClick(
    Windows::Foundation::IInspectable const&,
    Microsoft::UI::Xaml::RoutedEventArgs const&);

 private:
  void RebuildList();
  Microsoft::UI::Xaml::UIElement BuildRow(const std::string& appId);
  fire_and_forget InstallClicked(std::string appId);
  void LocateClicked(const std::string& appId);
  void ClearOverrideClicked(const std::string& appId);
  void ShowError(const std::string& message);
};

}  // namespace winrt::FlightSimHubApp::implementation

namespace winrt::FlightSimHubApp::factory_implementation {

struct EnvironmentPage
  : EnvironmentPageT<EnvironmentPage, implementation::EnvironmentPage> {};

}  // namespace winrt::FlightSimHubApp::factory_implementation
