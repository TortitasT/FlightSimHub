#include "pch.h"

#include "EnvironmentPage.xaml.h"
#if __has_include("EnvironmentPage.g.cpp")
#include "EnvironmentPage.g.cpp"
#endif

#include "FilePicker.h"
#include "Globals.h"

#include <FSHub/Installer.hpp>

#include <algorithm>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace FSHub;

namespace winrt::FlightSimHubApp::implementation {

namespace {

const AppDefinition& AppById(const std::string& appId) {
  auto& catalog = AppModel::Get().catalog;
  return *std::ranges::find_if(
    catalog, [&](const auto& app) { return app.id == appId; });
}

hstring StatusText(const InstallState& state) {
  switch (state.status) {
    case InstallState::Status::Detected:
      return L"Detected";
    case InstallState::Status::LocatedManually:
      return L"Located manually";
    default:
      return L"Not found";
  }
}

}  // namespace

EnvironmentPage::EnvironmentPage() {
  InitializeComponent();
  auto& model = AppModel::Get();
  if (model.settingsWereCorrupt) {
    ShowError(
      "The settings file was corrupt and has been reset; the old file was "
      "kept as settings.json.corrupt");
    model.settingsWereCorrupt = false;
  }
  RebuildList();
}

void EnvironmentPage::RebuildList() {
  AppList().Children().Clear();
  for (const auto& app: AppModel::Get().catalog) {
    AppList().Children().Append(BuildRow(app.id));
  }
}

UIElement EnvironmentPage::BuildRow(const std::string& appId) {
  auto& model = AppModel::Get();
  const auto& app = AppById(appId);
  const auto& state = model.states.at(appId);
  const bool found = state.status != InstallState::Status::NotFound;

  StackPanel info;
  info.Spacing(2);
  info.VerticalAlignment(VerticalAlignment::Center);

  TextBlock name;
  name.Text(to_hstring(
    app.name + (app.kind == AppKind::Sim ? "  (sim)" : "")));
  name.Style(
    Application::Current()
      .Resources()
      .Lookup(box_value(L"BodyStrongTextBlockStyle"))
      .as<Microsoft::UI::Xaml::Style>());
  info.Children().Append(name);

  TextBlock status;
  status.Text(
    StatusText(state)
    + (found ? L"    " + hstring {state.exePath.wstring()} : L""));
  status.Opacity(0.7);
  info.Children().Append(status);

  StackPanel buttons;
  buttons.Orientation(Orientation::Horizontal);
  buttons.Spacing(8);
  buttons.VerticalAlignment(VerticalAlignment::Center);
  buttons.HorizontalAlignment(HorizontalAlignment::Right);

  if (!found && app.source.type == SourceType::GitHub) {
    Button install;
    install.Content(box_value(L"Install"));
    install.Click([this, appId](auto&&, auto&&) { InstallClicked(appId); });
    buttons.Children().Append(install);
  }
  if (app.source.type == SourceType::Manual) {
    Button download;
    download.Content(box_value(L"Open download page"));
    download.Click([homepage = app.source.homepage](auto&&, auto&&) {
      ShellExecuteW(
        nullptr,
        L"open",
        winrt::to_hstring(homepage).c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL);
    });
    buttons.Children().Append(download);
  }
  Button locate;
  locate.Content(box_value(L"Locate..."));
  locate.Click([this, appId](auto&&, auto&&) { LocateClicked(appId); });
  buttons.Children().Append(locate);

  if (model.settings.appOverrides.contains(appId)) {
    Button clear;
    clear.Content(box_value(L"Clear override"));
    clear.Click(
      [this, appId](auto&&, auto&&) { ClearOverrideClicked(appId); });
    buttons.Children().Append(clear);
  }

  Grid row;
  row.Padding({12, 8, 12, 8});
  row.CornerRadius({4, 4, 4, 4});
  row.Background(
    Application::Current()
      .Resources()
      .Lookup(box_value(L"CardBackgroundFillColorDefaultBrush"))
      .as<Microsoft::UI::Xaml::Media::Brush>());
  ColumnDefinition infoColumn;
  infoColumn.Width({1, GridUnitType::Star});
  ColumnDefinition buttonColumn;
  buttonColumn.Width({0, GridUnitType::Auto});
  row.ColumnDefinitions().Append(infoColumn);
  row.ColumnDefinitions().Append(buttonColumn);
  Grid::SetColumn(info, 0);
  Grid::SetColumn(buttons, 1);
  row.Children().Append(info);
  row.Children().Append(buttons);
  return row;
}

void EnvironmentPage::OnRescanClick(
  IInspectable const&, RoutedEventArgs const&) {
  AppModel::Get().Rescan();
  RebuildList();
}

fire_and_forget EnvironmentPage::InstallClicked(std::string appId) {
  auto lifetime = get_strong();
  auto& model = AppModel::Get();
  const auto app = AppById(appId);

  ErrorBar().IsOpen(false);
  const auto queue = DispatcherQueue();

  co_await winrt::resume_background();
  const auto result = InstallApp(
    app, model.managedAppsDir, [](double) {
      // Row-level progress UI is a future nicety; installs show the
      // installer's own UI meanwhile
    });

  co_await wil::resume_foreground(queue);
  if (!result) {
    ShowError(app.name + ": " + result.error());
  }
  model.Rescan(appId);
  RebuildList();
}

void EnvironmentPage::LocateClicked(const std::string& appId) {
  const auto picked = PickExeFile();
  if (!picked) {
    return;
  }
  auto& model = AppModel::Get();
  model.settings.appOverrides[appId] = picked->string();
  model.Save();
  model.Rescan(appId);
  RebuildList();
}

void EnvironmentPage::ClearOverrideClicked(const std::string& appId) {
  auto& model = AppModel::Get();
  model.settings.appOverrides.erase(appId);
  model.Save();
  model.Rescan(appId);
  RebuildList();
}

void EnvironmentPage::ShowError(const std::string& message) {
  ErrorBar().Message(to_hstring(message));
  ErrorBar().IsOpen(true);
}

}  // namespace winrt::FlightSimHubApp::implementation
