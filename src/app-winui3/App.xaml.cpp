#include "pch.h"

#include "App.xaml.h"

#include "Globals.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::FlightSimHubApp::implementation {

App::App() {
  InitializeComponent();
#if defined _DEBUG \
  && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
  UnhandledException(
    [](IInspectable const&, UnhandledExceptionEventArgs const& e) {
      if (IsDebuggerPresent()) {
        auto message = e.Message();
        __debugbreak();
      }
    });
#endif
}

void App::OnLaunched(LaunchActivatedEventArgs const&) {
  try {
    FSHub::AppModel::Get().Load();
  } catch (const std::exception& e) {
    MessageBoxW(
      nullptr,
      to_hstring(e.what()).c_str(),
      L"FlightSimHub",
      MB_OK | MB_ICONERROR);
    Exit();
    return;
  }
  mWindow = make<MainWindow>();
  mWindow.Activate();
}

}  // namespace winrt::FlightSimHubApp::implementation
