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

// A button with a leading Fluent icon glyph and a text label, so every
// action across the app shares one icon vocabulary (Segoe Fluent Icons).
inline winrt::Microsoft::UI::Xaml::Controls::Button IconButton(
  const wchar_t* glyph, const winrt::hstring& label) {
  namespace mux = winrt::Microsoft::UI::Xaml;
  mux::Controls::FontIcon icon;
  icon.Glyph(glyph);
  icon.FontSize(16);
  mux::Controls::TextBlock text;
  text.Text(label);
  mux::Controls::StackPanel content;
  content.Orientation(mux::Controls::Orientation::Horizontal);
  content.Spacing(8);
  content.VerticalAlignment(mux::VerticalAlignment::Center);
  content.Children().Append(icon);
  content.Children().Append(text);
  mux::Controls::Button button;
  button.Content(content);
  return button;
}

// A compact icon-only button; the label survives as an accessible tooltip.
inline winrt::Microsoft::UI::Xaml::Controls::Button IconOnlyButton(
  const wchar_t* glyph, const winrt::hstring& tooltip) {
  namespace mux = winrt::Microsoft::UI::Xaml;
  mux::Controls::FontIcon icon;
  icon.Glyph(glyph);
  icon.FontSize(16);
  mux::Controls::Button button;
  button.Content(icon);
  mux::Controls::ToolTipService::SetToolTip(button, winrt::box_value(tooltip));
  return button;
}

// A small round presence indicator tinted by a theme fill brush.
inline winrt::Microsoft::UI::Xaml::Controls::Border StatusDot(
  const wchar_t* brushKey) {
  namespace mux = winrt::Microsoft::UI::Xaml;
  mux::Controls::Border dot;
  dot.Width(8);
  dot.Height(8);
  dot.CornerRadius({4, 4, 4, 4});
  dot.VerticalAlignment(mux::VerticalAlignment::Center);
  dot.Background(LookupResource<mux::Media::Brush>(brushKey));
  return dot;
}

inline winrt::hstring AppDisplayName(const AppDefinition& app) {
  return winrt::to_hstring(
    app.name + (app.kind == AppKind::Sim ? "  (sim)" : ""));
}

}  // namespace FSHub::Ui
