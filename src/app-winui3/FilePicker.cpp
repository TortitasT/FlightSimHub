#include "pch.h"

#include "FilePicker.h"

#include <shobjidl.h>

#include <wil/com.h>

namespace FSHub {

std::optional<std::filesystem::path> PickExeFile() {
  try {
    auto dialog
      = wil::CoCreateInstance<IFileOpenDialog>(CLSID_FileOpenDialog);

    constexpr COMDLG_FILTERSPEC filters[] = {
      {L"Programs (*.exe)", L"*.exe"},
      {L"All files (*.*)", L"*.*"},
    };
    dialog->SetFileTypes(ARRAYSIZE(filters), filters);
    dialog->SetTitle(L"Locate the application");

    if (FAILED(dialog->Show(GetActiveWindow()))) {
      return std::nullopt;
    }

    wil::com_ptr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item))) {
      return std::nullopt;
    }
    wil::unique_cotaskmem_string path;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
      return std::nullopt;
    }
    return std::filesystem::path {path.get()};
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace FSHub
