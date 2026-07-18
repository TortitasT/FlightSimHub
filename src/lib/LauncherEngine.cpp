#include <FSHub/LauncherEngine.hpp>

#include <algorithm>
#include <format>

namespace FSHub {

std::expected<std::vector<LaunchPlanItem>, std::string> BuildLaunchPlan(
  const Launcher& launcher,
  const std::vector<AppDefinition>& catalog,
  const std::map<std::string, InstallState>& states) {
  if (launcher.items.empty()) {
    return std::unexpected("the launcher has no programs");
  }

  std::vector<LaunchPlanItem> plan;
  size_t simCount = 0;
  for (const auto& item: launcher.items) {
    const auto app = std::ranges::find_if(
      catalog, [&](const auto& a) { return a.id == item.appId; });
    if (app == catalog.end()) {
      return std::unexpected(
        std::format("unknown app '{}' in launcher", item.appId));
    }

    const auto state = states.find(item.appId);
    if (
      state == states.end()
      || state->second.status == InstallState::Status::NotFound) {
      return std::unexpected(std::format(
        "{} is not installed or located; fix it in the Environment tab",
        app->name));
    }

    const bool isSim = app->kind == AppKind::Sim;
    simCount += isSim ? 1 : 0;
    plan.push_back({
      .appId = app->id,
      .name = app->name,
      .args = item.args,
      .exe = state->second.exePath,
      .delayAfterSeconds = item.delayAfterSeconds,
      .isSim = isSim,
      .gameProcessName = isSim ? app->gameProcessName : std::string {},
      .startTrackingButton
      = (!isSim && item.startTracking) ? app->startTrackingButton
                                       : std::string {},
    });
  }

  if (simCount != 1) {
    return std::unexpected(std::format(
      "a launcher needs exactly one sim, this one has {}", simCount));
  }
  return plan;
}

}  // namespace FSHub

#ifdef _WIN32

#include <FSHub/Strings.hpp>

#include <windows.h>

#include <tlhelp32.h>
#include <uiautomation.h>

#include <wil/com.h>
#include <wil/resource.h>

#include <cwctype>
#include <thread>

namespace FSHub {

namespace {

struct LaunchedProcess {
  DWORD pid {};
  wil::unique_handle handle;
  bool isSim {false};
};

std::expected<LaunchedProcess, std::string> StartProcess(
  const LaunchPlanItem& item) {
  // CreateProcessW requires a mutable command line buffer
  std::wstring commandLine = L"\"" + item.exe.wstring() + L"\"";
  if (!item.args.empty()) {
    commandLine += L" " + Widen(item.args);
  }

  const auto workingDir = item.exe.parent_path().wstring();
  STARTUPINFOW startup {.cb = sizeof(STARTUPINFOW)};
  PROCESS_INFORMATION process {};
  if (!CreateProcessW(
        item.exe.wstring().c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        workingDir.c_str(),
        &startup,
        &process)) {
    return std::unexpected(std::format(
      "failed to start {} (error {})", item.name, GetLastError()));
  }
  CloseHandle(process.hThread);
  return LaunchedProcess {
    .pid = process.dwProcessId,
    .handle = wil::unique_handle {process.hProcess},
    .isSim = item.isSim,
  };
}

bool IsProcessRunning(const std::wstring& exeName) {
  const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return false;
  }
  const wil::unique_handle snap {snapshot};
  PROCESSENTRY32W entry {.dwSize = sizeof(PROCESSENTRY32W)};
  if (Process32FirstW(snap.get(), &entry)) {
    do {
      if (_wcsicmp(entry.szExeFile, exeName.c_str()) == 0) {
        return true;
      }
    } while (Process32NextW(snap.get(), &entry));
  }
  return false;
}

// Waits for the game process to appear and then fully close. Used when the
// launched exe is a launcher that spawns the game separately, so the launcher
// exiting must not be mistaken for the game closing. Returns false if the
// game never started (e.g. the user closed the launcher without flying), in
// which case companions should be left alone.
bool WaitForGameProcess(const std::wstring& gameProcessName, HANDLE launcher) {
  constexpr DWORD kPollMs = 2'000;
  // A short grace after the launcher exits before giving up on the game
  // appearing; also debounces the handoff gap when a launcher relaunches the
  // game under the same process name (e.g. DCS.exe).
  constexpr int kMissesToClose = 3;

  bool launcherGone = false;
  int missesAfterLauncherGone = 0;
  while (!IsProcessRunning(gameProcessName)) {
    if (!launcherGone
        && WaitForSingleObject(launcher, 0) == WAIT_OBJECT_0) {
      launcherGone = true;
    }
    if (launcherGone && ++missesAfterLauncherGone >= kMissesToClose) {
      return false;  // launcher closed and no game ever started
    }
    Sleep(kPollMs);
  }

  int misses = 0;
  while (misses < kMissesToClose) {
    Sleep(kPollMs);
    misses = IsProcessRunning(gameProcessName) ? 0 : misses + 1;
  }
  return true;
}

std::wstring ToLower(std::wstring text) {
  for (auto& c: text) {
    c = static_cast<wchar_t>(std::towlower(c));
  }
  return text;
}

struct MainWindowSearch {
  DWORD pid {};
  HWND hwnd {nullptr};
};

BOOL CALLBACK FindVisibleTopLevelOfPid(HWND hwnd, LPARAM lparam) {
  auto& search = *reinterpret_cast<MainWindowSearch*>(lparam);
  DWORD windowPid = 0;
  GetWindowThreadProcessId(hwnd, &windowPid);
  if (
    windowPid == search.pid && IsWindowVisible(hwnd)
    && GetWindow(hwnd, GW_OWNER) == nullptr) {
    search.hwnd = hwnd;
    return FALSE;  // stop enumerating
  }
  return TRUE;
}

// Finds the first ENABLED descendant button whose (lowercased) name starts with
// namePrefix and invokes it. Returns true only if such a button was found and
// invoked. A disabled button (e.g. OpenTrack's "Start" once tracking runs, or a
// not-yet-initialized control) is skipped — so "no enabled match" doubles as the
// signal that the app either hasn't finished starting up or has already started.
bool InvokeEnabledStartButton(
  IUIAutomation* automation, HWND hwnd, const std::wstring& namePrefix) {
  wil::com_ptr<IUIAutomationElement> root;
  if (FAILED(automation->ElementFromHandle(hwnd, root.put())) || !root) {
    return false;
  }
  wil::unique_variant buttonType;
  buttonType.vt = VT_I4;
  buttonType.lVal = UIA_ButtonControlTypeId;
  wil::com_ptr<IUIAutomationCondition> isButton;
  if (FAILED(automation->CreatePropertyCondition(
        UIA_ControlTypePropertyId, buttonType, isButton.put()))) {
    return false;
  }
  wil::com_ptr<IUIAutomationElementArray> buttons;
  if (
    FAILED(root->FindAll(TreeScope_Descendants, isButton.get(), buttons.put()))
    || !buttons) {
    return false;
  }
  int count = 0;
  buttons->get_Length(&count);
  for (int i = 0; i < count; ++i) {
    wil::com_ptr<IUIAutomationElement> element;
    if (FAILED(buttons->GetElement(i, element.put())) || !element) {
      continue;
    }
    wil::unique_bstr name;
    if (FAILED(element->get_CurrentName(name.put())) || !name) {
      continue;
    }
    // Only names starting with the prefix (matches "Start"/"Start tracking",
    // excludes "Stop")
    if (ToLower(name.get()).rfind(namePrefix, 0) != 0) {
      continue;
    }
    // Skip a disabled Start button: it means not-ready-yet or already-started
    BOOL enabled = FALSE;
    if (FAILED(element->get_CurrentIsEnabled(&enabled)) || !enabled) {
      continue;
    }
    wil::com_ptr<IUIAutomationInvokePattern> invoke;
    if (
      SUCCEEDED(element->GetCurrentPatternAs(
        UIA_InvokePatternId,
        __uuidof(IUIAutomationInvokePattern),
        invoke.put_void()))
      && invoke) {
      return SUCCEEDED(invoke->Invoke());
    }
  }
  return false;
}

// Waits for the launched app's window, then clicks its start-tracking button.
// Best-effort: gives up silently if the window or button never appears. Runs on
// its own thread with its own (MTA) COM apartment.
void StartTrackingInApp(DWORD pid, std::wstring buttonPrefix) {
  if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
    return;
  }
  const auto uninit = wil::scope_exit([] { CoUninitialize(); });

  wil::com_ptr<IUIAutomation> automation;
  if (FAILED(CoCreateInstance(
        CLSID_CUIAutomation,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(automation.put())))) {
    return;
  }

  const std::wstring prefix = ToLower(buttonPrefix);
  // Click the Start button, then confirm it actually took — a single early
  // click can be lost while the app is still initializing (its Start button not
  // yet wired up), leaving tracking off. So retry until the enabled Start
  // button is gone: OpenTrack disables "Start" once tracking runs, AITrack
  // relabels/hides its window. Poll for up to ~30s, then give up.
  constexpr int kAttempts = 60;
  constexpr DWORD kIntervalMs = 500;
  constexpr DWORD kConfirmMs = 1200;
  bool clicked = false;
  for (int i = 0; i < kAttempts; ++i) {
    MainWindowSearch search {.pid = pid};
    EnumWindows(FindVisibleTopLevelOfPid, reinterpret_cast<LPARAM>(&search));

    if (!search.hwnd) {
      // No window: either still launching, or it vanished after starting
      // (AITrack hides on start). If we already clicked, treat as started.
      if (clicked) {
        return;
      }
      Sleep(kIntervalMs);
      continue;
    }

    if (InvokeEnabledStartButton(automation.get(), search.hwnd, prefix)) {
      // Clicked an enabled Start button; give the app a moment, then loop to
      // confirm the click stuck (Start became disabled / gone).
      clicked = true;
      Sleep(kConfirmMs);
      continue;
    }

    // Window present but no enabled Start button.
    if (clicked) {
      return;  // it took: Start is now disabled/gone
    }
    // Not started yet and Start not clickable (app still initializing); wait.
    Sleep(kIntervalMs);
  }
}

BOOL CALLBACK CloseWindowsOfTrackedPids(HWND hwnd, LPARAM lparam) {
  const auto& pids = *reinterpret_cast<const std::vector<DWORD>*>(lparam);
  DWORD windowPid = 0;
  GetWindowThreadProcessId(hwnd, &windowPid);
  if (std::ranges::find(pids, windowPid) != pids.end()) {
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
  }
  return TRUE;
}

}  // namespace

std::expected<void, std::string> LauncherEngine::Run(
  const std::vector<LaunchPlanItem>& plan, bool closeCompanions) {
  std::vector<LaunchedProcess> launched;
  HANDLE simHandle = nullptr;
  std::wstring gameProcessName;
  std::vector<std::thread> trackStarters;

  for (const auto& item: plan) {
    auto process = StartProcess(item);
    if (!process) {
      return std::unexpected(process.error());
    }
    if (process->isSim) {
      simHandle = process->handle.get();
      gameProcessName = Widen(item.gameProcessName);
    }
    if (!item.startTrackingButton.empty()) {
      // Off-thread: waiting for the app's window must not stall the launch
      // sequence or the remaining items' delays.
      trackStarters.emplace_back(
        StartTrackingInApp, process->pid, Widen(item.startTrackingButton));
    }
    launched.push_back(std::move(*process));
    if (item.delayAfterSeconds > 0) {
      Sleep(static_cast<DWORD>(item.delayAfterSeconds) * 1000);
    }
  }

  // Let the auto-start-tracking clicks finish before we settle into watching
  // the sim; they run concurrently and normally complete within seconds.
  for (auto& starter: trackStarters) {
    if (starter.joinable()) {
      starter.join();
    }
  }

  if (!closeCompanions || !simHandle) {
    return {};
  }

  if (gameProcessName.empty()) {
    // The launched process is the game itself.
    WaitForSingleObject(simHandle, INFINITE);
  } else if (!WaitForGameProcess(gameProcessName, simHandle)) {
    // A launcher was opened but the game never started; leave companions be.
    return {};
  }

  std::vector<DWORD> companionPids;
  for (const auto& process: launched) {
    if (
      !process.isSim
      && WaitForSingleObject(process.handle.get(), 0) == WAIT_TIMEOUT) {
      companionPids.push_back(process.pid);
    }
  }
  if (companionPids.empty()) {
    return {};
  }

  EnumWindows(
    CloseWindowsOfTrackedPids, reinterpret_cast<LPARAM>(&companionPids));

  constexpr DWORD kGraceMs = 10'000;
  for (const auto& process: launched) {
    if (process.isSim) {
      continue;
    }
    if (
      WaitForSingleObject(process.handle.get(), kGraceMs) == WAIT_TIMEOUT) {
      TerminateProcess(process.handle.get(), 0);
    }
  }
  return {};
}

}  // namespace FSHub

#endif  // _WIN32
