# FlightSimHub design

Date: 2026-07-16
Status: approved

## Purpose

A Windows desktop app that acts as a hub for flight sim needs, targeting DCS World and Falcon BMS. It has two responsibilities:

1. **Environment setup:** detect the two sims and a set of companion apps on the machine; for apps that are not installed, install them automatically (when a GitHub release exists) or guide the user to install them manually and locate the result.
2. **Launchers:** user-defined groups of companion apps plus one sim, launched in a chosen order with one click, with optional cleanup when the sim exits, and optional Start Menu shortcuts so a launcher can be started from Windows search.

## Tech stack

Same stack and conventions as FreeOpenKneeboard (`/Users/vgf/Git/freeopenkneeboard`):

- C++23, MSVC, Visual Studio CMake generator, CMake >= 3.25
- vcpkg manifest mode; dependencies: `cppwinrt`, `wil`, `nlohmann-json`
- WinUI 3 (Windows App SDK) with C++/WinRT for the UI
- Hybrid CRT linking (static C++ runtime + dynamic UCRT), mirroring OpenKneeboard's root CMakeLists approach
- JSON for all persisted data via nlohmann-json

Development happens on macOS; building and testing happen on a Windows machine. Nothing in this repo compiles on macOS.

## Managed applications

| Id | Name | Kind | Detection | Install source |
|----|------|------|-----------|----------------|
| `dcs` | DCS World | sim | Registry (`HKCU\Software\Eagle Dynamics\DCS World*`), Steam libraries, well-known paths | manual (storefront), Locate button |
| `falcon-bms` | Falcon BMS | sim | Registry (`HKLM\SOFTWARE\WOW6432Node\Benchmark Sims\Falcon BMS *`, `baseDir`), Steam libraries | manual, Locate button |
| `freeopenkneeboard` | FreeOpenKneeboard | companion | Registry uninstall keys, well-known paths | GitHub releases: `TortitasT/FreeOpenKneeboard` |
| `aitrack` | AITrack | companion | Well-known paths, managed dir | GitHub releases: `AIRLegend/aitrack` (portable zip) |
| `opentrack` | OpenTrack | companion | Registry uninstall keys, well-known paths | GitHub releases: `opentrack/opentrack` |
| `wdp` | Weapons Delivery Planner | companion (BMS) | Well-known paths | manual: open https://www.weapondeliveryplanner.nl, Locate button |
| `ezboards` | EzBoards | companion (BMS) | Well-known paths, managed dir | manual: open BMS forum thread, Locate button |

Install decisions from brainstorming: no winget; automatic installs always come from the GitHub Releases API. Manual apps get an "Open download page" button plus "Locate...". Portable apps (zip assets) are extracted to `%LOCALAPPDATA%\FlightSimHub\apps\<id>\`.

## Repository layout

```
FlightSimHub/
  CMakeLists.txt            root: toolchain policy, hybrid CRT, C++23, VS generator check
  CMakePresets.json
  vcpkg.json                cppwinrt, wil, nlohmann-json
  Directory.Build.props
  data/
    catalog.json            app catalog (data-driven definitions, schema below)
  src/
    lib/                    FSHub-Lib static library: all logic, no UI
    app-winui3/             FSHub-App: WinUI 3 executable
  tests/                    unit tests for FSHub-Lib pure logic
  docs/superpowers/specs/   this document
```

## Core components (src/lib)

### AppCatalog

Loads `catalog.json` (installed next to the exe). Schema per entry:

```json
{
  "id": "opentrack",
  "name": "OpenTrack",
  "kind": "companion",
  "exeName": "opentrack.exe",
  "detection": {
    "registryKeys": [{"root": "HKLM", "path": "...", "value": "InstallLocation"}],
    "wellKnownPaths": ["%ProgramFiles(x86)%/opentrack/opentrack.exe"],
    "steamAppId": null
  },
  "source": {
    "type": "github",
    "repo": "opentrack/opentrack",
    "assetPattern": "opentrack-.*-setup\\.exe",
    "installKind": "installer"
  }
}
```

`source.type` is `github` or `manual` (manual has `homepage` instead of repo/pattern). `installKind` is `installer` (run the downloaded exe/msi) or `portable` (unzip to the managed dir). Sims use `kind: "sim"` and detection may include `steamAppId` for Steam library scanning.

### Detector

For each catalog entry, resolves an install state:

- `Located` with exe path, from (in priority order): user override in config, registry hints, Steam library scan (parse `libraryfolders.vdf`, check `steamapps/common/...`), well-known paths, managed apps dir.
- `NotFound` otherwise.

Exposes `ScanAll()` and `Scan(appId)`. Pure resolution logic (priority merge, path expansion) is separated from the Windows probes (registry, filesystem) behind thin interfaces so it is unit-testable.

### Installer

For `github` sources:

1. `GET https://api.github.com/repos/<repo>/releases/latest` (WinRT `HttpClient`).
2. Choose the first asset matching `assetPattern`.
3. Download to `%TEMP%`, with progress reported to the UI.
4. `installer` kind: launch it and wait for exit. `portable` kind: extract the zip into `%LOCALAPPDATA%\FlightSimHub\apps\<id>\` (Windows shell zip extraction via IShellDispatch or minizip if needed; decided at implementation time, favoring no new dependencies).
5. Re-run detection for that app; failure states are surfaced with a retry option.

For `manual` sources: `Install` is replaced by `Open download page` (ShellExecute the homepage) and the user then uses `Locate...`.

### ConfigStore

`%LOCALAPPDATA%\FlightSimHub\settings.json`, nlohmann-json, written atomically (temp file + rename). Contents:

```json
{
  "appOverrides": {"wdp": "C:/.../WDP.exe"},
  "launchers": [
    {
      "id": "uuid",
      "name": "BMS Night Ops",
      "items": [
        {"appId": "opentrack", "args": "", "delayAfterSeconds": 3},
        {"appId": "freeopenkneeboard", "args": "", "delayAfterSeconds": 3},
        {"appId": "falcon-bms", "args": "", "delayAfterSeconds": 0}
      ],
      "closeCompanionsOnSimExit": true
    }
  ]
}
```

A corrupt file is renamed aside and defaults are used, with a visible warning. Serialization round-trip is unit-tested.

### LauncherEngine

- Validates the launcher first: every item must resolve to an existing exe; exactly one item must be a sim. Invalid items are reported without launching anything.
- Launches items in order via `CreateProcess` (working directory = exe directory), sleeping `delayAfterSeconds` after each.
- Tracks companion PIDs. Waits on the sim process handle; when the sim exits and `closeCompanionsOnSimExit` is true, posts `WM_CLOSE` to each tracked companion's top-level windows, then `TerminateProcess` after a grace period if still alive.
- Runs the wait on a background thread; the UI shows a "running" state per launcher.

### ShortcutWriter

Creates `%APPDATA%\Microsoft\Windows\Start Menu\Programs\FlightSimHub\<launcher name>.lnk` via `IShellLink`/`IPersistFile`, target `FlightSimHub.exe --launch <launcher-id>`. Also supports removing the shortcut when a launcher is deleted or renamed.

### CLI entry

`FlightSimHub.exe --launch <launcher-id>`: skips creating the main window, runs the LauncherEngine (including the sim-exit watcher), then exits. Any validation error shows a message box. No flag: normal windowed startup.

## UI (src/app-winui3)

WinUI 3 `NavigationView` with two pages, following OpenKneeboard's page structure (`.idl` + `.xaml` + code-behind per page):

- **Environment page:** one row per catalog app: icon, name, status badge (`Detected`, `Located manually`, `Not found`), resolved path, and actions: `Install` (github sources, with progress), `Open download page` (manual sources), `Locate...` (file picker, saved as override), `Clear override`. A `Rescan` button re-runs detection.
- **Launchers page:** list of launcher cards. Each card: name, item list in launch order with per-item delay, reorder up/down, add/remove item (picker limited to located apps), the sim highlighted, `closeCompanionsOnSimExit` toggle, `Launch` button, `Create Start Menu shortcut` button, delete launcher. `New launcher` button creates one.

## Error handling

- Download/API failures: inline error with `Retry`.
- Launch with missing exe: item flagged in the card, nothing launched.
- Config corruption: rename aside, defaults, warning bar.
- GitHub rate limiting (60/hr unauthenticated): errors surface the message; installs are user-initiated and rare, so no token support in v1.

## Testing

Unit tests (ctest) for FSHub-Lib pure logic only: catalog JSON parsing, config round-trip and corruption fallback, detection priority merge with fake probes, GitHub asset pattern selection, launcher validation rules. Windows API calls (registry, CreateProcess, IShellLink, HttpClient) live behind thin wrappers excluded from unit tests. Tests are compiled and run on Windows; they cannot run on the macOS dev machine.

## Out of scope (v1)

- Auto-update of FlightSimHub itself
- Updating already-installed companion apps to newer versions
- winget integration
- Per-launcher environment variables or pre/post scripts
- Localization
