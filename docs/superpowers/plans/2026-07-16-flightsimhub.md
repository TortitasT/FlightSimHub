# FlightSimHub Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Windows WinUI 3 hub app that detects/installs flight sim companion apps (DCS + Falcon BMS ecosystem) and runs user-defined launcher groups, with Start Menu shortcuts.

**Architecture:** `FSHub-Lib` static library holds all logic (catalog, detection, config, install, launch planning) with Windows API calls behind thin wrappers; `FSHub-App` is a WinUI 3 executable with two NavigationView pages (Environment, Launchers) plus a `--launch <id>` headless CLI path. Structure and CMake conventions mirror FreeOpenKneeboard.

**Tech Stack:** C++23, CMake >= 3.25 (Visual Studio generator only), vcpkg manifest (cppwinrt, wil, nlohmann-json, catch2 for tests), WinUI 3 via Windows App SDK NuGet references (`VS_PACKAGE_REFERENCES`), Hybrid CRT.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-07-16-flightsimhub-design.md`. Follow it exactly.
- C++23 (`CMAKE_CXX_STANDARD 23`), MSVC, Visual Studio generator required (fail otherwise, like OpenKneeboard).
- Hybrid CRT: `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"` plus `/DEFAULTLIB:ucrt$<$<CONFIG:Debug>:d>.lib` and `/NODEFAULTLIB:libucrt$<$<CONFIG:Debug>:d>.lib`.
- Windows App SDK 1.5.240607001, Windows SDK BuildTools 10.0.22621.756, cppwinrt 2.0.240111.5, WIL 1.0.250325.1 (versions copied from FreeOpenKneeboard).
- Namespace: C++ `FSHub`, WinRT root namespace `FlightSimHubApp`.
- Managed portable apps dir: `%LOCALAPPDATA%\FlightSimHub\apps\<id>\`. Settings: `%LOCALAPPDATA%\FlightSimHub\settings.json`.
- No em dash character anywhere (code, comments, commits, docs).
- Comments only for non-obvious whys; never restate code.
- IMPORTANT build/test caveat: this repo only compiles on Windows. When executing on a non-Windows machine, steps marked `Run on Windows:` cannot be executed; mark them as deferred and continue. Do not claim tests pass without running them.
- Commit after every task.

---

### Task 1: Build scaffolding

**Files:**
- Create: `.gitignore`, `vcpkg.json`, `vcpkg-configuration.json`, `CMakePresets.json`, `CMakeLists.txt`, `Directory.Build.props`, `cmake/windowsappsdk.cmake`, `src/CMakeLists.txt`, `src/lib/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces: static lib target `FSHub-Lib` (links `nlohmann_json::nlohmann_json`, public include dir `src/lib/include`), test target `FSHub-Tests` (Catch2, registered with ctest), cmake function `target_link_windows_app_sdk(<target>)`.

- [ ] **Step 1: Write `.gitignore`** with `build/`, `out/`, `.vs/`, `*.user`.

- [ ] **Step 2: Write `vcpkg.json`**

```json
{
  "builtin-baseline": "84bab45d415d22042bd0b9081aea57f362da3f35",
  "dependencies": ["cppwinrt", "wil", "nlohmann-json"],
  "features": {
    "tests": {
      "description": "Build unit tests",
      "dependencies": ["catch2"]
    }
  },
  "default-features": ["tests"],
  "overrides": [
    {"name": "cppwinrt", "version": "2.0.240111.5"},
    {"name": "wil", "version": "1.0.250325.1"}
  ]
}
```

Copy `vcpkg-configuration.json` from FreeOpenKneeboard so the baseline registry resolves.

- [ ] **Step 3: Write root `CMakeLists.txt`** mirroring OpenKneeboard's root: policies CMP0091/CMP0079, C++23, VS generator check (`FATAL_ERROR` if not Visual Studio), NTDDI Win10 19H1 defines, Hybrid CRT block (verbatim from Global Constraints), `project(FlightSimHub VERSION 0.1.0 LANGUAGES CXX)`, `add_subdirectory(cmake is not needed)`; include `cmake/windowsappsdk.cmake`, then `add_subdirectory(src)`, `enable_testing()`, `add_subdirectory(tests)`, copy `data/catalog.json` to the runtime output dir with `add_custom_command`/`configure_file`, `set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT FSHub-App)`.

- [ ] **Step 4: Write `cmake/windowsappsdk.cmake`**: simplified copy of FreeOpenKneeboard `third-party/windowsappsdk.cmake` with hardcoded versions from Global Constraints, providing `target_link_nuget_packages` and `target_link_windows_app_sdk` (sets `VS_PACKAGE_REFERENCES` for CppWinRT, WindowsAppSDK, SDK BuildTools, WIL; creates `Generated Files` include dir).

- [ ] **Step 5: Write `CMakePresets.json`** with one preset `default`: generator `Visual Studio 17 2022`, arch x64, toolchain `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`, binaryDir `${sourceDir}/build`. Add buildPresets `debug` (config Debug) and test preset `default`.

- [ ] **Step 6: Write `Directory.Build.props`** verbatim from FreeOpenKneeboard (NuGetTargetMoniker native, UseMultiToolTask).

- [ ] **Step 7: Write `src/CMakeLists.txt`** (`add_subdirectory(lib)`; app-winui3 added in Task 9), `src/lib/CMakeLists.txt` (empty `FSHub-Lib` static lib for now with placeholder source added in Task 2; declare target with sources listed per later tasks as they are created), `tests/CMakeLists.txt` (`find_package(Catch2 CONFIG REQUIRED)`, `add_executable(FSHub-Tests ...)`, `target_link_libraries(... FSHub-Lib Catch2::Catch2WithMain)`, `include(Catch)`, `catch_discover_tests(FSHub-Tests)`).

- [ ] **Step 8: Verify** Run on Windows: `cmake --preset default` completes. (On macOS: defer.)

- [ ] **Step 9: Commit** `git commit -m "build: scaffold CMake, vcpkg, Windows App SDK wiring"`

---

### Task 2: Catalog model and parsing (FSHub-Lib)

**Files:**
- Create: `src/lib/include/FSHub/AppDefinition.hpp`, `src/lib/AppDefinition.cpp`
- Test: `tests/CatalogTest.cpp`

**Interfaces:**
- Produces (namespace `FSHub`):

```cpp
enum class AppKind { Sim, Companion };
enum class SourceType { GitHub, Manual };
enum class InstallKind { Installer, Portable, None };

struct RegistryHint { std::string root; std::string path; std::string value; };

struct Detection {
  std::vector<RegistryHint> registryKeys;
  std::vector<std::string> wellKnownPaths;   // may contain %ENV% vars
  std::optional<std::string> steamRelativeExe; // under steamapps/common/
};

struct Source {
  SourceType type {SourceType::Manual};
  std::string repo;         // github: "owner/repo"
  std::string assetPattern; // github: ECMAScript regex on asset name
  InstallKind installKind {InstallKind::None};
  std::string homepage;     // manual
};

struct AppDefinition {
  std::string id, name, exeName;
  AppKind kind {AppKind::Companion};
  Detection detection;
  Source source;
};

// throws std::runtime_error with a message naming the bad entry/field
std::vector<AppDefinition> ParseCatalog(const nlohmann::json&);
```

- [ ] **Step 1: Write failing tests** in `tests/CatalogTest.cpp` (Catch2): parse a minimal 2-entry JSON (one sim/manual, one companion/github) and assert every field; missing `id` throws with `"id"` in message; unknown `kind` string throws; `github` source without `repo` throws; missing optional blocks (no registryKeys, no steamRelativeExe) parse to empty/nullopt.

- [ ] **Step 2:** Run on Windows: `ctest --preset default -R Catalog`, expect FAIL/compile error. (Defer on macOS.)

- [ ] **Step 3: Implement** `ParseCatalog` in `AppDefinition.cpp` with strict field access (`at()`), enum mapping via if-chains, and `try/catch` wrapping each entry to rethrow with the entry id or index in the message.

- [ ] **Step 4:** Run on Windows: same command, expect PASS. (Defer.)

- [ ] **Step 5: Commit** `git commit -m "feat(lib): app catalog model and JSON parsing"`

---

### Task 3: Shipped catalog data

**Files:**
- Create: `data/catalog.json`
- Test: extend `tests/CatalogTest.cpp` (parse the real file; path passed via `FSHUB_TEST_DATA_DIR` compile definition set in `tests/CMakeLists.txt`)

**Interfaces:**
- Produces: the 7 entries with ids exactly `dcs`, `falcon-bms`, `freeopenkneeboard`, `aitrack`, `opentrack`, `wdp`, `ezboards`.

- [ ] **Step 1: Write failing test**: load `${FSHUB_TEST_DATA_DIR}/catalog.json`, `ParseCatalog`, assert 7 entries, ids as above, exactly 2 sims, `freeopenkneeboard`/`aitrack`/`opentrack` are `SourceType::GitHub` and the rest `Manual`, `aitrack` is `InstallKind::Portable`.

- [ ] **Step 2: Write `data/catalog.json`** with the table from the spec:
  - `dcs`: sim, exe `bin/DCS.exe` relative marker `DCS.exe`; registry `HKCU\Software\Eagle Dynamics\DCS World` value `Path` and `...\DCS World OpenBeta` value `Path`; steamRelativeExe `DCSWorld/bin/DCS.exe`; wellKnownPaths `%ProgramFiles%/Eagle Dynamics/DCS World/bin/DCS.exe`; manual, homepage `https://www.digitalcombatsimulator.com/en/downloads/world/`.
  - `falcon-bms`: sim, exe `Falcon BMS.exe`; registry `HKLM\SOFTWARE\WOW6432Node\Benchmark Sims\Falcon BMS 4.37` value `baseDir` and same for `4.38` (exe under `Bin/x64/Falcon BMS.exe`); steamRelativeExe `Falcon BMS 4.37/Bin/x64/Falcon BMS.exe`; manual, homepage `https://www.falcon-bms.com/downloads/`.
  - `freeopenkneeboard`: companion, exe `OpenKneeboardApp.exe`; registry uninstall key `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OpenKneeboard` value `InstallLocation`; wellKnownPaths `%ProgramFiles%/OpenKneeboard/bin/OpenKneeboardApp.exe`; github `TortitasT/FreeOpenKneeboard`, assetPattern `(?i).*\\.(msi|exe)$`, installer.
  - `aitrack`: companion, exe `AITrack.exe`; wellKnownPaths empty; github `AIRLegend/aitrack`, assetPattern `(?i)aitrack.*\\.zip$`, portable.
  - `opentrack`: companion, exe `opentrack.exe`; registry uninstall `HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\opentrack` value `InstallLocation`; wellKnownPaths `%ProgramFiles(x86)%/opentrack/opentrack.exe`; github `opentrack/opentrack`, assetPattern `(?i).*install.*\\.exe$`, installer.
  - `wdp`: companion, exe `WDP.exe`; wellKnownPaths `%ProgramFiles(x86)%/WeaponDeliveryPlanner/WDP.exe`; manual, homepage `https://www.weapondeliveryplanner.nl`.
  - `ezboards`: companion, exe `EzBoards.exe`; wellKnownPaths empty; manual, homepage `https://forum.falcon-bms.com/topic/19901/`.

  Registry values that point at a directory rather than an exe get a per-hint optional `"append"` field (e.g. `baseDir` + `Bin/x64/Falcon BMS.exe`). Add `std::string append;` to `RegistryHint` in Task 2's header and parse it (default empty).

- [ ] **Step 3:** Run on Windows: `ctest -R Catalog`, expect PASS. (Defer.)

- [ ] **Step 4: Commit** `git commit -m "feat(data): shipped app catalog for DCS and Falcon BMS ecosystems"`

---

### Task 4: Settings model and store

**Files:**
- Create: `src/lib/include/FSHub/Settings.hpp`, `src/lib/Settings.cpp`
- Test: `tests/SettingsTest.cpp`

**Interfaces:**
- Produces (namespace `FSHub`):

```cpp
struct LauncherItem { std::string appId; std::string args; int delayAfterSeconds {3}; };
struct Launcher {
  std::string id;   // uuid string
  std::string name;
  std::vector<LauncherItem> items;
  bool closeCompanionsOnSimExit {true};
};
struct Settings {
  std::map<std::string, std::string> appOverrides; // appId -> exe path
  std::vector<Launcher> launchers;
};
void to_json(nlohmann::json&, const Settings&);
void from_json(const nlohmann::json&, Settings&);

struct LoadResult { Settings settings; bool wasCorrupt {false}; };
LoadResult LoadSettings(const std::filesystem::path& file);   // corrupt: rename to .corrupt, defaults
void SaveSettings(const std::filesystem::path& file, const Settings&); // temp + rename
std::string NewLauncherId(); // uuid v4 via CoCreateGuid on Windows, tested only for non-empty/uniqueness
```

- [ ] **Step 1: Write failing tests**: round-trip Settings -> json -> Settings equality; missing fields default (`delayAfterSeconds` 3, toggle true); `LoadSettings` on nonexistent file returns defaults, `wasCorrupt == false`; on invalid JSON file returns defaults, `wasCorrupt == true`, and original renamed to `settings.json.corrupt`; `SaveSettings` then `LoadSettings` round-trips (use a temp dir via `std::filesystem::temp_directory_path`).

- [ ] **Step 2:** Run on Windows: `ctest -R Settings`, expect FAIL. (Defer.)

- [ ] **Step 3: Implement** in `Settings.cpp`. `NewLauncherId` uses `CoCreateGuid` guarded by `#ifdef _WIN32`.

- [ ] **Step 4:** Run on Windows: expect PASS. (Defer.)

- [ ] **Step 5: Commit** `git commit -m "feat(lib): settings model, atomic store, corruption fallback"`

---

### Task 5: Detection

**Files:**
- Create: `src/lib/include/FSHub/Detector.hpp`, `src/lib/Detector.cpp`, `src/lib/include/FSHub/WindowsProbe.hpp`, `src/lib/WindowsProbe.cpp`
- Test: `tests/DetectorTest.cpp`

**Interfaces:**
- Produces:

```cpp
struct InstallState {
  enum class Status { Detected, LocatedManually, NotFound };
  Status status {Status::NotFound};
  std::filesystem::path exePath;
};

class IProbe {
 public:
  virtual ~IProbe() = default;
  virtual std::optional<std::string> ReadRegistryString(
    const std::string& root, const std::string& path, const std::string& value) const = 0;
  virtual bool FileExists(const std::filesystem::path&) const = 0;
  virtual std::string ExpandEnvironmentVars(const std::string&) const = 0;
  virtual std::optional<std::string> ReadTextFile(const std::filesystem::path&) const = 0;
};

// pure, testable
std::vector<std::filesystem::path> ParseSteamLibraryFolders(std::string_view vdfText);

class Detector {
 public:
  Detector(const IProbe& probe, std::filesystem::path managedAppsDir);
  // override wins (LocatedManually); then registry, steam, wellKnown, managed dir (Detected)
  InstallState Resolve(const AppDefinition& app,
                       const std::optional<std::filesystem::path>& userOverride) const;
 private:
  std::vector<std::filesystem::path> SteamCommonDirs() const; // reads libraryfolders.vdf via probe
};

class WindowsProbe final : public IProbe { /* Win32 registry + filesystem, no unit tests */ };
```

- [ ] **Step 1: Write failing tests** with a `FakeProbe` (in the test file, maps for registry values, file set, env expansion): override present and file exists -> `LocatedManually`; registry hint with `append` joins path; registry dir hint whose exe is missing falls through to wellKnownPaths; Steam vdf parsing extracts multiple `"path"` entries (give a realistic `libraryfolders.vdf` literal); app found under `<lib>/steamapps/common/<steamRelativeExe>` -> `Detected`; portable app found under `managedAppsDir/<id>/<exeName>` -> `Detected`; nothing anywhere -> `NotFound`; override pointing at a missing file is ignored (falls through to detection).

- [ ] **Step 2:** Run on Windows: `ctest -R Detector`, expect FAIL. (Defer.)

- [ ] **Step 3: Implement** `Detector.cpp` (pure logic against `IProbe`) and `ParseSteamLibraryFolders` (regex `"path"\s+"([^"]+)"`, unescape `\\\\`). Steam root comes from registry `HKCU\Software\Valve\Steam` value `SteamPath` via the probe.

- [ ] **Step 4: Implement `WindowsProbe.cpp`**: `RegGetValueW` (map `HKCU`/`HKLM` roots, `RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY`), `std::filesystem::exists`, `ExpandEnvironmentStringsW`, `std::ifstream` read. Wrap wide/narrow conversion with `winrt::to_string`/`winrt::to_hstring` or `MultiByteToWideChar` helpers in an anonymous namespace. Guard whole file with `#ifdef _WIN32` so the lib still lists it unconditionally.

- [ ] **Step 5:** Run on Windows: expect PASS. (Defer.)

- [ ] **Step 6: Commit** `git commit -m "feat(lib): app detection with registry, steam and path probes"`

---

### Task 6: GitHub installer

**Files:**
- Create: `src/lib/include/FSHub/Installer.hpp`, `src/lib/Installer.cpp`
- Test: `tests/InstallerTest.cpp` (asset selection only)

**Interfaces:**
- Produces:

```cpp
struct ReleaseAsset { std::string name; std::string downloadUrl; };
// release: GitHub /releases/latest JSON. Returns first asset whose name matches pattern.
std::optional<ReleaseAsset> SelectAsset(const nlohmann::json& release, const std::string& assetPattern);

// Windows-only (guarded #ifdef _WIN32), used by the app:
// - FetchLatestReleaseJson(repo) -> nlohmann::json (WinRT HttpClient, User-Agent "FlightSimHub")
// - DownloadFile(url, destFile, progressCallback(double 0..1))
// - RunInstallerAndWait(exePath) (CreateProcess + WaitForSingleObject)
// - ExtractZip(zipFile, destDir) (IShellDispatch Folder.CopyHere, flags 4|16|512|1024)
// - InstallApp(const AppDefinition&, managedAppsDir, progressCallback) -> std::expected<void, std::string>
```

- [ ] **Step 1: Write failing tests** for `SelectAsset`: picks first match among several assets; case-insensitive pattern from catalog works against `OpenKneeboard-v1.2.msi`; returns `nullopt` when nothing matches or `assets` missing.

- [ ] **Step 2:** Run on Windows: `ctest -R Installer`, expect FAIL. (Defer.)

- [ ] **Step 3: Implement** `SelectAsset` with `std::regex` (ECMAScript, `icase` only when pattern starts with `(?i)`; strip that prefix since std::regex lacks inline flags). Then implement the Windows-only functions.

- [ ] **Step 4:** Run on Windows: expect PASS. (Defer.)

- [ ] **Step 5: Commit** `git commit -m "feat(lib): github release installer with download and zip extraction"`

---

### Task 7: Launch planning and engine

**Files:**
- Create: `src/lib/include/FSHub/LauncherEngine.hpp`, `src/lib/LauncherEngine.cpp`
- Test: `tests/LaunchPlanTest.cpp`

**Interfaces:**
- Produces:

```cpp
struct LaunchPlanItem {
  std::string appId, name, args;
  std::filesystem::path exe;
  int delayAfterSeconds {0};
  bool isSim {false};
};

// pure, testable: validates and resolves; error string names the problem app
std::expected<std::vector<LaunchPlanItem>, std::string> BuildLaunchPlan(
  const Launcher& launcher,
  const std::vector<AppDefinition>& catalog,
  const std::map<std::string, InstallState>& states);

// Windows-only engine
class LauncherEngine {
 public:
  // Blocking: launches in order with delays; if closeCompanions, waits for the
  // sim to exit then WM_CLOSE companions' top-level windows, TerminateProcess after 10s.
  static std::expected<void, std::string> Run(
    const std::vector<LaunchPlanItem>& plan, bool closeCompanions);
};
```

- [ ] **Step 1: Write failing tests** for `BuildLaunchPlan`: happy path resolves exe paths and sim flag from catalog kind; unknown appId -> error containing the id; item whose state is `NotFound` -> error containing the app name; zero sims -> error; two sims -> error; empty items -> error; delays and args copied through.

- [ ] **Step 2:** Run on Windows: `ctest -R LaunchPlan`, expect FAIL. (Defer.)

- [ ] **Step 3: Implement** `BuildLaunchPlan`, then `LauncherEngine::Run` (Windows-only block): `CreateProcessW` with working dir = exe parent, keep `PROCESS_INFORMATION` handles, `Sleep(delay*1000)` between items, wait on sim handle, `EnumWindows` matching companion PIDs to post `WM_CLOSE`, grace wait, `TerminateProcess`.

- [ ] **Step 4:** Run on Windows: expect PASS. (Defer.)

- [ ] **Step 5: Commit** `git commit -m "feat(lib): launch plan validation and process engine"`

---

### Task 8: Start Menu shortcuts

**Files:**
- Create: `src/lib/include/FSHub/ShortcutWriter.hpp`, `src/lib/ShortcutWriter.cpp` (Windows-only, no unit tests: pure COM/filesystem)

**Interfaces:**
- Produces:

```cpp
// %APPDATA%\Microsoft\Windows\Start Menu\Programs\FlightSimHub\<name>.lnk
// target: current exe, args: --launch <launcherId>
std::expected<void, std::string> CreateStartMenuShortcut(
  const std::string& launcherName, const std::string& launcherId);
std::expected<void, std::string> RemoveStartMenuShortcut(const std::string& launcherName);
```

- [ ] **Step 1: Implement** with `SHGetKnownFolderPath(FOLDERID_Programs)`, create `FlightSimHub` subdir, `CoCreateInstance(CLSID_ShellLink)`, `IShellLinkW::SetPath(GetModuleFileNameW result)`, `SetArguments`, `IPersistFile::Save`. Sanitize the launcher name for the filename (strip `\/:*?"<>|`).

- [ ] **Step 2:** Run on Windows: build compiles. (Defer.)

- [ ] **Step 3: Commit** `git commit -m "feat(lib): start menu shortcut writer"`

---

### Task 9: WinUI 3 app shell

**Files:**
- Create: `src/app-winui3/CMakeLists.txt`, `src/app-winui3/winui3-boilerplate.cmake`, `src/app-winui3/Directory.Build.targets` (copy from FreeOpenKneeboard), `src/app-winui3/pch.h`, `src/app-winui3/app.manifest`, `src/app-winui3/App.idl`, `App.xaml`, `App.xaml.h`, `App.xaml.cpp`, `MainWindow.idl`, `MainWindow.xaml`, `MainWindow.xaml.h`, `MainWindow.xaml.cpp`, `src/app-winui3/Main.cpp`
- Modify: `src/CMakeLists.txt` (add subdirectory)

**Interfaces:**
- Consumes: `FSHub-Lib` everything above.
- Produces: `FSHub-App` exe (`OUTPUT_NAME FlightSimHub`), `DISABLE_XAML_GENERATED_MAIN` with our own `wWinMain` in `Main.cpp` that parses `--launch <id>` (headless path: load catalog + settings, `WindowsProbe` + `Detector`, `BuildLaunchPlan`, `LauncherEngine::Run`, `MessageBoxW` on error, exit) or starts the XAML `App`. A shared `AppModel` singleton struct in `Globals.h/cpp` holding catalog, settings, detector results for the pages.

- [ ] **Step 1:** Write CMake files: adapt OpenKneeboard's `winui3-boilerplate.cmake` (same `VS_GLOBAL_*` block, idl DependentUpon loop, `module.g.cpp`), `ok_add_executable` replaced by plain `add_executable(FSHub-App WIN32 ...)` + `target_link_windows_app_sdk`, definitions `DISABLE_XAML_GENERATED_MAIN` and `MICROSOFT_WINDOWSAPPSDK_UNDOCKEDREGFREEWINRT_AUTO_INITIALIZE_LOADLIBRARY`, `App.xaml` marked `VS_XAML_TYPE ApplicationDefinition`.

- [ ] **Step 2:** Write `pch.h` (winrt WinUI headers, wil, unknwn.h first), `app.manifest` (copy OpenKneeboard's, rename to FlightSimHub, keep dpiAware PerMonitorV2 and common-controls), minimal `App` class (standard WinUI3 template shape: `OnLaunched` creates `MainWindow`), `MainWindow.xaml` with `NavigationView` (PaneDisplayMode Top, two `NavigationViewItem`s: Environment, Launchers; `Frame` content) and code-behind navigating to the pages (pages exist after Tasks 10/11; navigate defensively so the shell compiles first with a placeholder `TextBlock`).

- [ ] **Step 3:** Write `Main.cpp` `wWinMain`: if `--launch` in `GetCommandLineW` args, headless run as described; else `winrt::Microsoft::UI::Xaml::Application::Start` with `App`.

- [ ] **Step 4:** Run on Windows: `cmake --build --preset debug` produces `FlightSimHub.exe` showing the window. (Defer.)

- [ ] **Step 5: Commit** `git commit -m "feat(app): winui3 shell with navigation and --launch CLI"`

---

### Task 10: Environment page

**Files:**
- Create: `src/app-winui3/EnvironmentPage.idl`, `.xaml`, `.xaml.h`, `.xaml.cpp`, `src/app-winui3/FilePicker.h/.cpp` (adapt OpenKneeboard's IFileDialog wrapper, simplified)
- Modify: `src/app-winui3/CMakeLists.txt`, `MainWindow.xaml.cpp` (navigate)

**Interfaces:**
- Consumes: `AppModel` (catalog, settings, `Detector`), `InstallApp`, `SaveSettings`.
- Produces: page listing every catalog app: status badge text (`Detected` / `Located manually` / `Not found`), resolved path, buttons per source type: `Install` with `ProgressBar` (github + NotFound), `Open download page` (manual), `Locate...` (all), `Clear override` (when override set), top `Rescan` button. Locate saves `appOverrides[appId]` and re-resolves. Install runs `InstallApp` on a background thread (winrt coroutine `co_await winrt::resume_background()`), then rescans; failure shows `InfoBar` with Retry.

- [ ] **Step 1:** Implement XAML: `ItemsControl`/`ListView` with a `DataTemplate` bound to a `FSHubApp.AppEntryViewModel` runtime class (idl: Name, StatusText, PathText, CanInstall, IsManual, HasOverride booleans + PropertyChanged) plus click handlers wired by name.
- [ ] **Step 2:** Implement code-behind + view model updates; persist settings on every override change.
- [ ] **Step 3:** Run on Windows: manual check per spec UI section. (Defer.)
- [ ] **Step 4: Commit** `git commit -m "feat(app): environment page with detect, install, locate"`

---

### Task 11: Launchers page

**Files:**
- Create: `src/app-winui3/LaunchersPage.idl`, `.xaml`, `.xaml.h`, `.xaml.cpp`
- Modify: `src/app-winui3/CMakeLists.txt`, `MainWindow.xaml.cpp`

**Interfaces:**
- Consumes: `AppModel`, `BuildLaunchPlan`, `LauncherEngine::Run`, `CreateStartMenuShortcut`, `RemoveStartMenuShortcut`, `SaveSettings`, `NewLauncherId`.
- Produces: launcher cards (`ItemsControl` of expanders): editable name `TextBox`, item list with app `ComboBox` (located apps only), args `TextBox`, delay `NumberBox`, up/down/remove buttons, add-item button, `ToggleSwitch` closeCompanionsOnSimExit, buttons `Launch` (disabled while running; runs plan on background thread, `InfoBar` on validation error), `Create Start Menu shortcut`, `Delete` (also removes shortcut). `New launcher` command bar button.

- [ ] **Step 1:** Implement XAML + `FSHubApp.LauncherViewModel`/`LauncherItemViewModel` runtime classes mirroring the Settings structs.
- [ ] **Step 2:** Implement code-behind: every edit writes back to `AppModel.settings` and `SaveSettings`; Launch builds plan fresh from current detection states.
- [ ] **Step 3:** Run on Windows: create a launcher with two dummy apps (e.g. notepad located manually), Launch, verify order and cleanup toggle; create shortcut, verify it appears in Start Menu search and `--launch` works. (Defer.)
- [ ] **Step 4: Commit** `git commit -m "feat(app): launchers page with ordered launch and shortcuts"`

---

### Task 12: README and docs

**Files:**
- Create: `README.md`

- [ ] **Step 1:** Write README: what it is, screenshots placeholder, build requirements (VS 2022 with C++ workload + Windows App SDK, CMake >= 3.25, vcpkg with `VCPKG_ROOT` set), build commands (`cmake --preset default`, `cmake --build --preset debug`), test command (`ctest --preset default`), architecture pointer to the spec.
- [ ] **Step 2: Commit** `git commit -m "docs: README with build instructions"`

## Verification (on Windows)

1. `cmake --preset default && cmake --build --preset debug` clean.
2. `ctest --preset default` all green.
3. Manual: Environment page detects an installed sim; Install works for OpenTrack; Locate works for WDP; launcher with OpenTrack + BMS starts in order; closing BMS closes OpenTrack when toggle on; Start Menu shortcut launches headless.
