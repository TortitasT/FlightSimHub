#include "pch.h"

#include "LaunchersPage.xaml.h"
#if __has_include("LaunchersPage.g.cpp")
#include "LaunchersPage.g.cpp"
#endif

#include "Globals.h"
#include "UiHelpers.h"

#include <FSHub/LauncherEngine.hpp>
#include <FSHub/ShortcutWriter.hpp>

#include <algorithm>
#include <cmath>
#include <thread>

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
  const auto* launcher = LauncherById(launcherId);
  const auto& item = launcher->items.at(itemIndex);

  // A dense single horizontal line overflowed the card in a portrait window.
  // Lay the item out on two width-adaptive lines inside its own sub-card:
  //   line 1:  [ app picker .......................... ]  [up][down][remove]
  //   line 2:  [ arguments ................ ] [delay]  [x] Start tracking
  const auto starColumn = [] {
    ColumnDefinition c;
    c.Width({1, GridUnitType::Star});
    return c;
  };
  const auto autoColumn = [] {
    ColumnDefinition c;
    c.Width({0, GridUnitType::Auto});
    return c;
  };

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

  // --- Line 1: app picker (stretches) + reorder/remove actions ---
  ComboBox appPicker;
  appPicker.HorizontalAlignment(HorizontalAlignment::Stretch);
  const auto apps = LaunchableApps();
  int selected = -1;
  for (size_t i = 0; i < apps.size(); ++i) {
    appPicker.Items().Append(box_value(Ui::AppDisplayName(*apps[i])));
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
    [this, launcherId, itemIndex, apps](auto&& sender, auto&&) {
      const auto index = sender.template as<ComboBox>().SelectedIndex();
      if (index < 0 || index >= static_cast<int>(apps.size())) {
        return;
      }
      auto* launcher = LauncherById(launcherId);
      if (!launcher || itemIndex >= launcher->items.size()) {
        return;
      }
      launcher->items.at(itemIndex).appId = apps[index]->id;
      AppModel::Get().Save();
      // Deferred: rebuilding synchronously would tear down this ComboBox
      // while its own event is still being raised. The rebuild drops the
      // stale "(not found)" placeholder entry.
      DispatcherQueue().TryEnqueue([weak = get_weak()] {
        if (auto self = weak.get()) {
          self->RebuildList();
        }
      });
    });

  // Fluent glyphs by codepoint; a literal arrow character here depends on the
  // compiler's source-charset guess. Icon-only, with the label as a tooltip.
  auto up = Ui::IconOnlyButton(L"\uE70E", L"Move up");
  up.Click([moveItem](auto&&, auto&&) { moveItem(-1); });
  auto down = Ui::IconOnlyButton(L"\uE70D", L"Move down");
  down.Click([moveItem](auto&&, auto&&) { moveItem(1); });
  auto remove = Ui::IconOnlyButton(L"\uE738", L"Remove");
  remove.Click([this, launcherId, itemIndex](auto&&, auto&&) {
    auto* launcher = LauncherById(launcherId);
    if (!launcher || itemIndex >= launcher->items.size()) {
      return;
    }
    launcher->items.erase(launcher->items.begin() + itemIndex);
    AppModel::Get().Save();
    RebuildList();
  });

  StackPanel actions;
  actions.Orientation(Orientation::Horizontal);
  actions.Spacing(4);
  actions.VerticalAlignment(VerticalAlignment::Center);
  actions.Children().Append(up);
  actions.Children().Append(down);
  actions.Children().Append(remove);

  Grid topRow;
  topRow.ColumnSpacing(8);
  topRow.ColumnDefinitions().Append(starColumn());
  topRow.ColumnDefinitions().Append(autoColumn());
  Grid::SetColumn(appPicker, 0);
  Grid::SetColumn(actions, 1);
  topRow.Children().Append(appPicker);
  topRow.Children().Append(actions);

  // --- Line 2: arguments (stretches) + delay + start-tracking toggle ---
  TextBox args;
  args.Header(box_value(L"Arguments"));
  args.PlaceholderText(L"optional");
  args.HorizontalAlignment(HorizontalAlignment::Stretch);
  args.Text(to_hstring(item.args));
  // LostFocus, not TextChanged: persisting per keystroke writes the whole
  // settings file for every character typed
  args.LostFocus([launcherId, itemIndex](auto&& sender, auto&&) {
    auto* launcher = LauncherById(launcherId);
    if (!launcher || itemIndex >= launcher->items.size()) {
      return;
    }
    auto& item = launcher->items.at(itemIndex);
    const auto text = to_string(sender.template as<TextBox>().Text());
    if (text != item.args) {
      item.args = text;
      AppModel::Get().Save();
    }
  });

  NumberBox delay;
  delay.Header(box_value(L"Delay (s)"));
  delay.SpinButtonPlacementMode(NumberBoxSpinButtonPlacementMode::Compact);
  delay.MinWidth(116);
  delay.Minimum(0);
  delay.Maximum(600);
  delay.Value(item.delayAfterSeconds);
  delay.ValueChanged([launcherId, itemIndex](auto&&, auto&& eventArgs) {
    auto* launcher = LauncherById(launcherId);
    if (!launcher || itemIndex >= launcher->items.size()) {
      return;
    }
    const auto value = eventArgs.NewValue();
    launcher->items.at(itemIndex).delayAfterSeconds
      = std::isnan(value) ? 0 : static_cast<int>(value);
    AppModel::Get().Save();
  });

  Grid bottomRow;
  bottomRow.ColumnSpacing(8);
  bottomRow.ColumnDefinitions().Append(starColumn());
  bottomRow.ColumnDefinitions().Append(autoColumn());
  bottomRow.ColumnDefinitions().Append(autoColumn());
  Grid::SetColumn(args, 0);
  Grid::SetColumn(delay, 1);
  bottomRow.Children().Append(args);
  bottomRow.Children().Append(delay);

  // Auto-start-tracking toggle, shown only for apps that expose a start button
  // (e.g. OpenTrack, AITrack). Bottom-aligned to sit on the input baseline.
  const auto& catalog = AppModel::Get().catalog;
  const auto appIt = std::ranges::find_if(
    catalog, [&](const auto& a) { return a.id == item.appId; });
  if (appIt != catalog.end() && !appIt->startTrackingButton.empty()) {
    CheckBox track;
    track.Content(box_value(L"Start tracking"));
    track.VerticalAlignment(VerticalAlignment::Bottom);
    track.Margin({0, 0, 0, 6});
    // Set state before wiring events so this doesn't trigger a spurious save
    track.IsChecked(item.startTracking);
    const auto setTracking = [launcherId, itemIndex](bool on) {
      auto* launcher = LauncherById(launcherId);
      if (!launcher || itemIndex >= launcher->items.size()) {
        return;
      }
      launcher->items.at(itemIndex).startTracking = on;
      AppModel::Get().Save();
    };
    track.Checked([setTracking](auto&&, auto&&) { setTracking(true); });
    track.Unchecked([setTracking](auto&&, auto&&) { setTracking(false); });
    Grid::SetColumn(track, 2);
    bottomRow.Children().Append(track);
  }

  StackPanel content;
  content.Spacing(8);
  content.Children().Append(topRow);
  content.Children().Append(bottomRow);

  // Its own subtle sub-card so items read as distinct blocks in the list
  Border container;
  container.Padding({12, 10, 12, 10});
  container.CornerRadius({4, 4, 4, 4});
  container.Background(Ui::LookupResource<Media::Brush>(
    L"CardBackgroundFillColorSecondaryBrush"));
  container.Child(content);
  return container;
}

UIElement LaunchersPage::BuildCard(const std::string& launcherId) {
  const auto* launcher = LauncherById(launcherId);
  const bool running = mRunning.contains(launcherId);

  StackPanel body;
  body.Spacing(8);

  TextBox name;
  name.Header(box_value(L"Name"));
  name.HorizontalAlignment(HorizontalAlignment::Stretch);
  name.Text(to_hstring(launcher->name));
  name.LostFocus([launcherId](auto&& sender, auto&&) {
    auto* launcher = LauncherById(launcherId);
    if (!launcher) {
      return;
    }
    const auto newName = to_string(sender.template as<TextBox>().Text());
    if (newName == launcher->name || newName.empty()) {
      return;
    }
    // Shortcuts are keyed by name; migrate an existing one across the rename
    const bool hadShortcut = StartMenuShortcutExists(launcher->name);
    if (hadShortcut) {
      RemoveStartMenuShortcut(launcher->name);
    }
    launcher->name = newName;
    AppModel::Get().Save();
    if (hadShortcut) {
      CreateStartMenuShortcut(launcher->name, launcher->id);
    }
  });
  body.Children().Append(name);

  TextBlock itemsHeader;
  itemsHeader.Text(L"Programs, in launch order (exactly one sim):");
  body.Children().Append(itemsHeader);

  for (size_t i = 0; i < launcher->items.size(); ++i) {
    body.Children().Append(BuildItemRow(launcherId, i));
  }

  auto addItem = Ui::IconButton(L"\uE710", L"Add program");
  addItem.Click([this, launcherId](auto&&, auto&&) {
    const auto apps = LaunchableApps();
    if (auto* launcher = LauncherById(launcherId); launcher && !apps.empty()) {
      launcher->items.push_back({.appId = apps.front()->id});
      AppModel::Get().Save();
      RebuildList();
    } else {
      Ui::ShowError(
        ErrorBar(),
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

  auto launch = Ui::IconButton(
    running ? L"\uE72C" : L"\uE768", running ? L"Running\u2026" : L"Launch");
  launch.IsEnabled(!running);
  launch.Style(Ui::LookupResource<Microsoft::UI::Xaml::Style>(
    L"AccentButtonStyle"));
  launch.Click(
    [this, launcherId](auto&&, auto&&) { LaunchClicked(launcherId); });
  actions.Children().Append(launch);

  const bool hasShortcut = StartMenuShortcutExists(launcher->name);
  auto shortcut = Ui::IconButton(
    hasShortcut ? L"\uE77A" : L"\uE718",
    hasShortcut ? L"Remove Start Menu shortcut"
                : L"Add Start Menu shortcut");
  shortcut.Click([this, launcherId, hasShortcut](auto&&, auto&&) {
    const auto* launcher = LauncherById(launcherId);
    if (!launcher) {
      return;
    }
    const auto result = hasShortcut
      ? RemoveStartMenuShortcut(launcher->name)
      : CreateStartMenuShortcut(launcher->name, launcher->id);
    if (!result) {
      Ui::ShowError(ErrorBar(), result.error());
      return;
    }
    RebuildList();  // refresh the button to reflect the new state
  });
  actions.Children().Append(shortcut);

  auto remove = Ui::IconButton(L"\uE74D", L"Delete launcher");
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

  return Ui::MakeCard(body);
}

void LaunchersPage::LaunchClicked(const std::string& launcherId) {
  if (mRunning.contains(launcherId)) {
    return;
  }
  const auto* launcher = LauncherById(launcherId);
  if (!launcher) {
    return;
  }
  ErrorBar().IsOpen(false);
  mRunning.insert(launcherId);
  RebuildList();

  auto& model = AppModel::Get();
  // Copies: the thread must not touch UI-owned state, and it outlives
  // any edit the user makes while the sim runs
  const Launcher snapshot = *launcher;
  const auto overrides = model.settings.appOverrides;
  const auto queue = DispatcherQueue();
  auto weak = get_weak();

  // A dedicated thread, not the WinRT thread pool: with cleanup enabled
  // this blocks until the sim exits, which can be hours
  std::thread([snapshot, overrides, queue, weak] {
    auto& model = AppModel::Get();
    const auto states = model.ComputeStates(overrides);

    std::optional<std::string> error;
    const auto plan = BuildLaunchPlan(snapshot, model.catalog, states);
    if (!plan) {
      error = plan.error();
    } else if (const auto result = LauncherEngine::Run(
                 *plan, snapshot.closeCompanionsOnSimExit);
               !result) {
      error = result.error();
    }

    queue.TryEnqueue([weak, error, id = snapshot.id] {
      if (auto self = weak.get()) {
        self->mRunning.erase(id);
        if (error) {
          Ui::ShowError(self->ErrorBar(), *error);
        }
        self->RebuildList();
      }
    });
  }).detach();
}

}  // namespace winrt::FlightSimHubApp::implementation
