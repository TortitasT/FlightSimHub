#include "pch.h"

#include "EnvironmentPage.xaml.h"
#if __has_include("EnvironmentPage.g.cpp")
#include "EnvironmentPage.g.cpp"
#endif

#include "FilePicker.h"
#include "Globals.h"
#include "UiHelpers.h"

#include <FSHub/Installer.hpp>
#include <FSHub/Strings.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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
    Ui::ShowError(
      ErrorBar(),
      "The settings file was corrupt and has been reset; the old file was "
      "kept as settings.json.corrupt");
    model.settingsWereCorrupt = false;
  }
  RebuildList();
}

void EnvironmentPage::RebuildList() {
  AppList().Children().Clear();
  const auto& catalog = AppModel::Get().catalog;

  // The section a companion falls under: its group, defaulting to "general".
  const auto groupOf = [](const AppDefinition& app) -> std::string {
    return app.group.empty() ? std::string {"general"} : app.group;
  };

  // Sections run one per sim (in catalog order), then a General section for
  // cross-sim tools. A sim heads its own section (keyed by its id); companions
  // are placed by their group.
  std::vector<std::pair<std::string, hstring>> sections;
  for (const auto& app: catalog) {
    if (app.kind == AppKind::Sim) {
      sections.push_back({app.id, to_hstring(app.name)});
    }
  }
  sections.push_back({"general", L"General"});

  for (const auto& [key, title]: sections) {
    std::vector<const AppDefinition*> members;
    for (const auto& app: catalog) {
      const std::string section
        = app.kind == AppKind::Sim ? app.id : groupOf(app);
      if (section == key) {
        members.push_back(&app);
      }
    }
    if (members.empty()) {
      continue;
    }
    AppList().Children().Append(BuildSectionHeader(title));
    for (const auto* app: members) {
      AppList().Children().Append(BuildRow(app->id));
    }
  }
}

UIElement EnvironmentPage::BuildSectionHeader(const hstring& title) {
  TextBlock header;
  header.Text(title);
  header.Style(Ui::LookupResource<Microsoft::UI::Xaml::Style>(
    L"BodyStrongTextBlockStyle"));
  // Breathing room above each section; the list's own spacing handles the rest.
  header.Margin({0, 8, 0, 0});
  return header;
}

UIElement EnvironmentPage::BuildRow(const std::string& appId) {
  auto& model = AppModel::Get();
  const auto& app = AppById(appId);
  const auto& state = model.states.at(appId);
  const bool found = state.status != InstallState::Status::NotFound;

  // A leading icon tile identifies the app at a glance: an airplane for
  // sims, the apps-grid glyph for companions. One icon family throughout.
  FontIcon kindIcon;
  kindIcon.Glyph(app.kind == AppKind::Sim ? L"\uE709" : L"\uE71D");
  kindIcon.FontSize(20);
  Border iconTile;
  iconTile.Width(40);
  iconTile.Height(40);
  iconTile.CornerRadius({8, 8, 8, 8});
  iconTile.VerticalAlignment(VerticalAlignment::Center);
  iconTile.Background(Ui::LookupResource<Media::Brush>(
    L"ControlFillColorSecondaryBrush"));
  iconTile.Child(kindIcon);

  StackPanel info;
  info.Spacing(2);
  info.VerticalAlignment(VerticalAlignment::Center);

  TextBlock name;
  name.Text(Ui::AppDisplayName(app));
  name.Style(
    Ui::LookupResource<Microsoft::UI::Xaml::Style>(L"BodyStrongTextBlockStyle"));
  info.Children().Append(name);

  // Presence at a glance: green detected, blue located by hand, amber missing
  const wchar_t* dotBrush
    = state.status == InstallState::Status::Detected
      ? L"SystemFillColorSuccessBrush"
    : state.status == InstallState::Status::LocatedManually
      ? L"SystemFillColorAttentionBrush"
      : L"SystemFillColorCautionBrush";

  TextBlock status;
  status.Text(
    StatusText(state)
    + (found ? L"    " + hstring {state.exePath.wstring()} : L""));
  status.Opacity(0.7);

  StackPanel statusRow;
  statusRow.Orientation(Orientation::Horizontal);
  statusRow.Spacing(8);
  statusRow.VerticalAlignment(VerticalAlignment::Center);
  statusRow.Children().Append(Ui::StatusDot(dotBrush));
  statusRow.Children().Append(status);
  info.Children().Append(statusRow);

  StackPanel buttons;
  buttons.Orientation(Orientation::Horizontal);
  buttons.Spacing(8);
  buttons.VerticalAlignment(VerticalAlignment::Center);
  buttons.HorizontalAlignment(HorizontalAlignment::Right);

  if (!found && app.source.type == SourceType::GitHub) {
    auto install = Ui::IconButton(L"\uE896", L"Install");
    install.Click([this, appId](auto&&, auto&&) { InstallClicked(appId); });
    buttons.Children().Append(install);
  }
  if (!found && app.source.type == SourceType::Manual) {
    auto download = Ui::IconButton(L"\uE774", L"Open download page");
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
  auto locate = Ui::IconButton(L"\uE8E5", L"Locate\u2026");
  locate.Click([this, appId](auto&&, auto&&) { LocateClicked(appId); });
  buttons.Children().Append(locate);

  if (model.settings.appOverrides.contains(appId)) {
    auto clear = Ui::IconButton(L"\uE7A7", L"Clear override");
    clear.Click(
      [this, appId](auto&&, auto&&) { ClearOverrideClicked(appId); });
    buttons.Children().Append(clear);
  }

  Grid row;
  ColumnDefinition iconColumn;
  iconColumn.Width({0, GridUnitType::Auto});
  ColumnDefinition infoColumn;
  infoColumn.Width({1, GridUnitType::Star});
  ColumnDefinition buttonColumn;
  buttonColumn.Width({0, GridUnitType::Auto});
  row.ColumnDefinitions().Append(iconColumn);
  row.ColumnDefinitions().Append(infoColumn);
  row.ColumnDefinitions().Append(buttonColumn);
  row.ColumnSpacing(12);
  Grid::SetColumn(iconTile, 0);
  Grid::SetColumn(info, 1);
  Grid::SetColumn(buttons, 2);
  row.Children().Append(iconTile);
  row.Children().Append(info);
  row.Children().Append(buttons);
  auto card = Ui::MakeCard(row);
  card.Padding({12, 8, 12, 8});
  return card;
}

fire_and_forget EnvironmentPage::OnRescanClick(
  IInspectable const&, RoutedEventArgs const&) {
  auto lifetime = get_strong();
  auto& model = AppModel::Get();
  const auto overrides = model.settings.appOverrides;
  const auto queue = DispatcherQueue();

  co_await winrt::resume_background();
  auto states = model.ComputeStates(overrides);

  co_await wil::resume_foreground(queue);
  model.states = std::move(states);
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
    Ui::ShowError(ErrorBar(), app.name + ": " + result.error());
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
  // UTF-8 in settings.json; path::string() would encode as ACP
  model.settings.appOverrides[appId] = Narrow(picked->wstring());
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

}  // namespace winrt::FlightSimHubApp::implementation
