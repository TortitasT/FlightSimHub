#pragma once

#include <filesystem>
#include <optional>

namespace FSHub {

// Modal IFileOpenDialog filtered to *.exe; returns nullopt on cancel.
std::optional<std::filesystem::path> PickExeFile();

}  // namespace FSHub
