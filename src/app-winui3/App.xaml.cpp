#include "pch.h"

#include "App.xaml.h"

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
  mWindow = make<MainWindow>();
  mWindow.Activate();
}

}  // namespace winrt::FlightSimHubApp::implementation
