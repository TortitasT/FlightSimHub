#include "pch.h"

#include "App.xaml.h"
#include "Globals.h"

#include <FSHub/LauncherEngine.hpp>

#include <shellapi.h>

#include <algorithm>

namespace {

int Fail(const std::wstring& message) {
  MessageBoxW(nullptr, message.c_str(), L"FlightSimHub", MB_OK | MB_ICONERROR);
  return EXIT_FAILURE;
}

// Exit code doubles as the error signal for shell-launched shortcuts
int RunHeadlessLauncher(const std::wstring& launcherId) {
  using namespace FSHub;
  try {
    auto& model = AppModel::Get();
    model.Load();

    const std::string id
      = winrt::to_string(winrt::hstring {launcherId});
    const auto launcher = std::ranges::find_if(
      model.settings.launchers,
      [&](const auto& l) { return l.id == id; });
    if (launcher == model.settings.launchers.end()) {
      return Fail(
        L"This launcher no longer exists; recreate its shortcut from "
        L"FlightSimHub.");
    }

    const auto plan
      = BuildLaunchPlan(*launcher, model.catalog, model.states);
    if (!plan) {
      return Fail(std::wstring {winrt::to_hstring(plan.error())});
    }

    const auto result
      = LauncherEngine::Run(*plan, launcher->closeCompanionsOnSimExit);
    if (!result) {
      return Fail(std::wstring {winrt::to_hstring(result.error())});
    }
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    return Fail(std::wstring {winrt::to_hstring(e.what())});
  }
}

}  // namespace

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::wstring launcherId;
  for (int i = 1; i < argc - 1; ++i) {
    if (std::wstring_view {argv[i]} == L"--launch") {
      launcherId = argv[i + 1];
    }
  }
  LocalFree(argv);

  if (!launcherId.empty()) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    return RunHeadlessLauncher(launcherId);
  }

  winrt::init_apartment(winrt::apartment_type::single_threaded);
  ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) {
    ::winrt::make<::winrt::FlightSimHubApp::implementation::App>();
  });
  return 0;
}
