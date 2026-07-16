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

#include <wil/resource.h>

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

  for (const auto& item: plan) {
    auto process = StartProcess(item);
    if (!process) {
      return std::unexpected(process.error());
    }
    if (process->isSim) {
      simHandle = process->handle.get();
    }
    launched.push_back(std::move(*process));
    if (item.delayAfterSeconds > 0) {
      Sleep(static_cast<DWORD>(item.delayAfterSeconds) * 1000);
    }
  }

  if (!closeCompanions || !simHandle) {
    return {};
  }

  WaitForSingleObject(simHandle, INFINITE);

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
