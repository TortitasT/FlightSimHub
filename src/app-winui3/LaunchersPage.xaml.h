#pragma once

#include "LaunchersPage.xaml.g.h"

#include <string>

namespace winrt::FlightSimHubApp::implementation {

struct LaunchersPage : LaunchersPageT<LaunchersPage> {
  LaunchersPage();

  void OnNewLauncherClick(
    Windows::Foundation::IInspectable const&,
    Microsoft::UI::Xaml::RoutedEventArgs const&);

 private:
  void RebuildList();
  Microsoft::UI::Xaml::UIElement BuildCard(const std::string& launcherId);
  Microsoft::UI::Xaml::UIElement BuildItemRow(
    const std::string& launcherId, size_t itemIndex);
  fire_and_forget LaunchClicked(std::string launcherId);
  void ShowError(const std::string& message);
};

}  // namespace winrt::FlightSimHubApp::implementation

namespace winrt::FlightSimHubApp::factory_implementation {

struct LaunchersPage
  : LaunchersPageT<LaunchersPage, implementation::LaunchersPage> {};

}  // namespace winrt::FlightSimHubApp::factory_implementation
