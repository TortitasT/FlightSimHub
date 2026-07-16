# FlightSimHub

A hub for your flight sim needs, targeting **DCS World** and **Falcon BMS**.

FlightSimHub does two things:

1. **Environment**: detects your sims and companion apps, and installs the ones you are missing (or points you at their download page and lets you locate them manually):
   - [FreeOpenKneeboard](https://github.com/TortitasT/FreeOpenKneeboard)
   - [AITrack](https://github.com/AIRLegend/aitrack)
   - [OpenTrack](https://github.com/opentrack/opentrack)
   - [Weapons Delivery Planner](https://www.weapondeliveryplanner.nl) (Falcon BMS)
   - [EzBoards](https://forum.falcon-bms.com/topic/19901/) (Falcon BMS)
2. **Launchers**: one-click groups of companion apps plus a sim, started in the order you choose with per-app delays and arguments. Optionally closes the companions when the sim exits, and can create a Start Menu shortcut so a launcher is one Windows-search keystroke away (the shortcut runs `FlightSimHub.exe --launch <id>` headless).

Built with the same stack as [FreeOpenKneeboard](https://github.com/TortitasT/FreeOpenKneeboard): C++23, CMake + vcpkg, WinUI 3 (Windows App SDK) with C++/WinRT.

## Building

Requirements:

- Windows 10 19H1 or later
- Visual Studio 2022 with the "Desktop development with C++" workload
- CMake 3.25 or later (the Visual Studio generator is required)
- [vcpkg](https://github.com/microsoft/vcpkg), with the `VCPKG_ROOT` environment variable set

```powershell
cmake --preset default
cmake --build --preset debug
```

The app lands in `build\src\app-winui3\Debug\FlightSimHub.exe` with `catalog.json` beside it.

## Tests

Unit tests cover the core library (catalog parsing, detection resolution, settings persistence, release asset selection, launch plan validation):

```powershell
ctest --preset default
```

## Layout

- `src/lib`: `FSHub-Lib`, all logic, no UI. Windows API calls sit behind thin wrappers (`IProbe`, installer/engine/shortcut functions) so the logic is unit-testable.
- `src/app-winui3`: `FSHub-App`, the WinUI 3 app (Environment and Launchers pages) plus the `--launch` CLI path.
- `data/catalog.json`: data-driven definitions of the managed apps (detection hints and install sources).
- `docs/superpowers/specs`: the design document.

Settings live in `%LOCALAPPDATA%\FlightSimHub\settings.json`; portable apps installed by the hub go to `%LOCALAPPDATA%\FlightSimHub\apps\<id>\`.
