#pragma once

#include <FSHub/AppDefinition.hpp>

#include <string>

namespace FSHub::Ui {

template <class T>
T LookupResource(const wchar_t* key) {
  return winrt::Microsoft::UI::Xaml::Application::Current()
    .Resources()
    .Lookup(winrt::box_value(key))
    .as<T>();
}

inline winrt::Microsoft::UI::Xaml::Controls::Border MakeCard(
  const winrt::Microsoft::UI::Xaml::UIElement& content) {
  winrt::Microsoft::UI::Xaml::Controls::Border card;
  card.Padding({12, 12, 12, 12});
  card.CornerRadius({4, 4, 4, 4});
  card.Background(LookupResource<winrt::Microsoft::UI::Xaml::Media::Brush>(
    L"CardBackgroundFillColorDefaultBrush"));
  card.Child(content);
  return card;
}

inline void ShowError(
  const winrt::Microsoft::UI::Xaml::Controls::InfoBar& bar,
  const std::string& message) {
  bar.Message(winrt::to_hstring(message));
  bar.IsOpen(true);
}

inline winrt::hstring AppDisplayName(const AppDefinition& app) {
  return winrt::to_hstring(
    app.name + (app.kind == AppKind::Sim ? "  (sim)" : ""));
}

}  // namespace FSHub::Ui
