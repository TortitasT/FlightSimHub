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

  StackPanel row;
  row.Orientation(Orientation::Horizontal);
  row.Spacing(8);

  ComboBox appPicker;
  appPicker.MinWidth(220);
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
      const auto index
        = sender.template as<ComboBox>().SelectedIndex();
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
  row.Children().Append(appPicker);

  TextBox args;
  args.PlaceholderText(L"Arguments");
  args.MinWidth(160);
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
  row.Children().Append(args);

  NumberBox delay;
  delay.PlaceholderText(L"Delay (s)");
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

  // Fluent glyphs by codepoint; a literal arrow character here depends on
  // the compiler's source-charset guess. Icon-only in this dense row, with
  // the label preserved as a tooltip.
  auto up = Ui::IconOnlyButton(L"\uE70E", L"Move up");
  up.Click([moveItem](auto&&, auto&&) { moveItem(-1); });
  row.Children().Append(up);

  auto down = Ui::IconOnlyButton(L"\uE70D", L"Move down");
  down.Click([moveItem](auto&&, auto&&) { moveItem(1); });
  row.Children().Append(down);

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
  row.Children().Append(remove);

  return row;
}

UIElement LaunchersPage::BuildCard(const std::string& launcherId) {
  const auto* launcher = LauncherById(launcherId);
  const bool running = mRunning.contains(launcherId);

  StackPanel body;
  body.Spacing(8);

  TextBox name;
  name.Header(box_value(L"Name"));
  name.MaxWidth(320);
  name.HorizontalAlignment(HorizontalAlignment::Left);
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

  auto shortcut = Ui::IconButton(L"\uE718", L"Create Start Menu shortcut");
  shortcut.Click([this, launcherId](auto&&, auto&&) {
    if (const auto* launcher = LauncherById(launcherId)) {
      if (const auto result
          = CreateStartMenuShortcut(launcher->name, launcher->id);
          !result) {
        Ui::ShowError(ErrorBar(), result.error());
      }
    }
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
