#include "pch.h"

#include "LaunchersPage.xaml.h"
#if __has_include("LaunchersPage.g.cpp")
#include "LaunchersPage.g.cpp"
#endif

#include "Globals.h"

#include <FSHub/LauncherEngine.hpp>
#include <FSHub/ShortcutWriter.hpp>

#include <algorithm>
#include <cmath>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace FSHub;

namespace winrt::FlightSimHubApp::implementation {

namespace {

Launcher* LauncherById(const std::string& launcherId) {
  auto& launchers = AppModel::Get().settings.launchers;
  const auto it = std::ranges::find_if(
    launchers, [&](const auto& l) { return l.id == launcherId; });
  return it == launchers.end() ? nullptr : &*it;
}

// Apps that can be added to a launcher: anything currently located
std::vector<const AppDefinition*> LaunchableApps() {
  auto& model = AppModel::Get();
  std::vector<const AppDefinition*> apps;
  for (const auto& app: model.catalog) {
    if (
      model.states.at(app.id).status != InstallState::Status::NotFound) {
      apps.push_back(&app);
    }
  }
  return apps;
}

}  // namespace

LaunchersPage::LaunchersPage() {
  InitializeComponent();
  RebuildList();
}

void LaunchersPage::RebuildList() {
  LauncherList().Children().Clear();
  for (const auto& launcher: AppModel::Get().settings.launchers) {
    LauncherList().Children().Append(BuildCard(launcher.id));
  }
}

void LaunchersPage::OnNewLauncherClick(
  IInspectable const&, RoutedEventArgs const&) {
  auto& model = AppModel::Get();
  model.settings.launchers.push_back(Launcher {
    .id = NewLauncherId(),
    .name = "New launcher",
  });
  model.Save();
  RebuildList();
}

UIElement LaunchersPage::BuildItemRow(
  const std::string& launcherId, size_t itemIndex) {
  auto& model = AppModel::Get();
  const auto* launcher = LauncherById(launcherId);
  const auto& item = launcher->items.at(itemIndex);

  StackPanel row;
  row.Orientation(Orientation::Horizontal);
  row.Spacing(8);

  ComboBox appPicker;
  appPicker.MinWidth(220);
  const auto apps = LaunchableApps();
  int selected = -1;
  for (size_t i = 0; i < apps.size(); ++i) {
    appPicker.Items().Append(box_value(to_hstring(
      apps[i]->name + (apps[i]->kind == AppKind::Sim ? "  (sim)" : ""))));
    if (apps[i]->id == item.appId) {
      selected = static_cast<int>(i);
    }
  }
  if (selected == -1 && !item.appId.empty()) {
    // The configured app is currently NotFound; keep it visible
    appPicker.Items().Append(
      box_value(to_hstring(item.appId + "  (not found)")));
    selected = static_cast<int>(apps.size());
  }
  appPicker.SelectedIndex(selected);
  appPicker.SelectionChanged(
    [launcherId, itemIndex, apps](auto&& sender, auto&&) {
      const auto index
        = sender.template as<ComboBox>().SelectedIndex();
      if (index < 0 || index >= static_cast<int>(apps.size())) {
        return;
      }
      if (auto* launcher = LauncherById(launcherId)) {
        launcher->items.at(itemIndex).appId = apps[index]->id;
        AppModel::Get().Save();
      }
    });
  row.Children().Append(appPicker);

  TextBox args;
  args.PlaceholderText(L"Arguments");
  args.MinWidth(160);
  args.Text(to_hstring(item.args));
  args.TextChanged([launcherId, itemIndex](auto&& sender, auto&&) {
    if (auto* launcher = LauncherById(launcherId)) {
      launcher->items.at(itemIndex).args
        = to_string(sender.template as<TextBox>().Text());
      AppModel::Get().Save();
    }
  });
  row.Children().Append(args);

  NumberBox delay;
  delay.Header(box_value(L""));
  delay.PlaceholderText(L"Delay (s)");
  delay.Minimum(0);
  delay.Maximum(600);
  delay.Value(item.delayAfterSeconds);
  delay.ValueChanged([launcherId, itemIndex](auto&&, auto&& eventArgs) {
    if (auto* launcher = LauncherById(launcherId)) {
      const auto value = eventArgs.NewValue();
      launcher->items.at(itemIndex).delayAfterSeconds
        = std::isnan(value) ? 0 : static_cast<int>(value);
      AppModel::Get().Save();
    }
  });
  row.Children().Append(delay);

  const auto moveItem = [this, launcherId, itemIndex](int direction) {
    auto* launcher = LauncherById(launcherId);
    const auto target = static_cast<int>(itemIndex) + direction;
    if (
      !launcher || target < 0
      || target >= static_cast<int>(launcher->items.size())) {
      return;
    }
    std::swap(launcher->items.at(itemIndex), launcher->items.at(target));
    AppModel::Get().Save();
    RebuildList();
  };

  Button up;
  up.Content(box_value(L"↑"));
  up.Click([moveItem](auto&&, auto&&) { moveItem(-1); });
  row.Children().Append(up);

  Button down;
  down.Content(box_value(L"↓"));
  down.Click([moveItem](auto&&, auto&&) { moveItem(1); });
  row.Children().Append(down);

  Button remove;
  remove.Content(box_value(L"Remove"));
  remove.Click([this, launcherId, itemIndex](auto&&, auto&&) {
    if (auto* launcher = LauncherById(launcherId)) {
      launcher->items.erase(launcher->items.begin() + itemIndex);
      AppModel::Get().Save();
      RebuildList();
    }
  });
  row.Children().Append(remove);

  return row;
}

UIElement LaunchersPage::BuildCard(const std::string& launcherId) {
  const auto* launcher = LauncherById(launcherId);

  StackPanel body;
  body.Spacing(8);

  TextBox name;
  name.Header(box_value(L"Name"));
  name.MaxWidth(320);
  name.HorizontalAlignment(HorizontalAlignment::Left);
  name.Text(to_hstring(launcher->name));
  name.TextChanged([launcherId](auto&& sender, auto&&) {
    if (auto* launcher = LauncherById(launcherId)) {
      launcher->name = to_string(sender.template as<TextBox>().Text());
      AppModel::Get().Save();
    }
  });
  body.Children().Append(name);

  TextBlock itemsHeader;
  itemsHeader.Text(L"Programs, in launch order (exactly one sim):");
  body.Children().Append(itemsHeader);

  for (size_t i = 0; i < launcher->items.size(); ++i) {
    body.Children().Append(BuildItemRow(launcherId, i));
  }

  Button addItem;
  addItem.Content(box_value(L"Add program"));
  addItem.Click([this, launcherId](auto&&, auto&&) {
    const auto apps = LaunchableApps();
    if (auto* launcher = LauncherById(launcherId); launcher && !apps.empty()) {
      launcher->items.push_back({.appId = apps.front()->id});
      AppModel::Get().Save();
      RebuildList();
    } else {
      ShowError(
        "No located apps to add; install or locate apps in the Environment "
        "tab first");
    }
  });
  body.Children().Append(addItem);

  ToggleSwitch closeToggle;
  closeToggle.Header(
    box_value(L"Close companion apps when the sim exits"));
  closeToggle.IsOn(launcher->closeCompanionsOnSimExit);
  closeToggle.Toggled([launcherId](auto&& sender, auto&&) {
    if (auto* launcher = LauncherById(launcherId)) {
      launcher->closeCompanionsOnSimExit
        = sender.template as<ToggleSwitch>().IsOn();
      AppModel::Get().Save();
    }
  });
  body.Children().Append(closeToggle);

  StackPanel actions;
  actions.Orientation(Orientation::Horizontal);
  actions.Spacing(8);

  Button launch;
  launch.Content(box_value(L"Launch"));
  launch.Style(
    Application::Current()
      .Resources()
      .Lookup(box_value(L"AccentButtonStyle"))
      .as<Microsoft::UI::Xaml::Style>());
  launch.Click(
    [this, launcherId](auto&&, auto&&) { LaunchClicked(launcherId); });
  actions.Children().Append(launch);

  Button shortcut;
  shortcut.Content(box_value(L"Create Start Menu shortcut"));
  shortcut.Click([this, launcherId](auto&&, auto&&) {
    if (const auto* launcher = LauncherById(launcherId)) {
      if (const auto result
          = CreateStartMenuShortcut(launcher->name, launcher->id);
          !result) {
        ShowError(result.error());
      }
    }
  });
  actions.Children().Append(shortcut);

  Button remove;
  remove.Content(box_value(L"Delete launcher"));
  remove.Click([this, launcherId](auto&&, auto&&) {
    auto& model = AppModel::Get();
    if (const auto* launcher = LauncherById(launcherId)) {
      RemoveStartMenuShortcut(launcher->name);
      std::erase_if(
        model.settings.launchers,
        [&](const auto& l) { return l.id == launcherId; });
      model.Save();
      RebuildList();
    }
  });
  actions.Children().Append(remove);

  body.Children().Append(actions);

  Border card;
  card.Padding({12, 12, 12, 12});
  card.CornerRadius({4, 4, 4, 4});
  card.Background(
    Application::Current()
      .Resources()
      .Lookup(box_value(L"CardBackgroundFillColorDefaultBrush"))
      .as<Microsoft::UI::Xaml::Media::Brush>());
  card.Child(body);
  return card;
}

fire_and_forget LaunchersPage::LaunchClicked(std::string launcherId) {
  auto lifetime = get_strong();
  auto& model = AppModel::Get();

  ErrorBar().IsOpen(false);
  const auto* launcher = LauncherById(launcherId);
  if (!launcher) {
    co_return;
  }

  model.Rescan();
  const auto plan = BuildLaunchPlan(*launcher, model.catalog, model.states);
  if (!plan) {
    ShowError(plan.error());
    co_return;
  }
  const bool closeCompanions = launcher->closeCompanionsOnSimExit;

  const auto queue = DispatcherQueue();
  co_await winrt::resume_background();
  // Blocks until the sim exits when cleanup is enabled
  const auto result = LauncherEngine::Run(*plan, closeCompanions);

  co_await wil::resume_foreground(queue);
  if (!result) {
    ShowError(result.error());
  }
}

void LaunchersPage::ShowError(const std::string& message) {
  ErrorBar().Message(to_hstring(message));
  ErrorBar().IsOpen(true);
}

}  // namespace winrt::FlightSimHubApp::implementation
