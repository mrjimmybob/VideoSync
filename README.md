# VideoSync

A small Windows system-tray helper that watches one or more folders and
automatically moves video files into matching destination folders (such as a
NAS share), replicating the directory tree as it goes.

It runs quietly in the system tray — no main window — with a blue icon when idle
and a green icon while syncing.

> **Note:** VideoSync *moves* files (not copies). The source folder is emptied as
> files are transferred. See [Caveats](#caveats).

## Features

- **Watches multiple folders** — each source/destination pair is configurable.
- **Automatic** — a file-system watcher triggers a sync shortly after files land.
- **Debounced** — waits for a burst of activity to settle before syncing, so a
  multi-file download triggers a single pass.
- **Periodic safety re-scan** — re-checks every source on a timer in case a
  file-system event is missed.
- **Manual sync** — *Sync Now* in the tray menu.
- **Optional logging** — off by default; appends timestamped activity to a file.
- **Simple INI config** — hand-editable, created automatically on first run.

## Requirements

- Windows
- Qt 6 (built/tested against **Qt 6.11, MinGW**)

## Building

The simplest path is the included batch scripts (they use the Qt-bundled
CMake / Ninja / MinGW toolchain — edit the paths at the top of `build.bat` if
your Qt install differs):

- **`build.bat`** — builds a release and runs `windeployqt`, producing a
  ready-to-run folder in `build\release\` (exe + Qt DLLs + plugins).
- **`box.bat`** — one-click: runs `build.bat`, then packages everything into a
  single standalone `build\Publish\VideoSync_boxed.exe` (see
  [Single-file build](#single-file-build-boxed)).

To build manually instead, with Qt and MinGW on your `PATH`:

```sh
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64"
cmake --build build
```

Or open `CMakeLists.txt` in Qt Creator with the MinGW kit.

### Single-file build (boxed)

`box.bat` (or `make_box.ps1` on its own, after `build.bat`) uses
[Enigma Virtual Box](https://enigmaprotector.com/en/aboutvb.html) to embed the
Qt runtime into one portable `VideoSync_boxed.exe` — no DLLs to ship.

`make_box.ps1` regenerates `VideoSync.evb` from `build\release` and boxes only
the runtime (DLLs + plugin folders). It deliberately **does not** box
`VideoSync.ini` or `VideoSync.log`, so the config and log remain external,
editable files next to the boxed exe. To distribute, copy
`VideoSync_boxed.exe` and place a `VideoSync.ini` beside it (one is generated on
first run if missing).

## Configuration

VideoSync reads `VideoSync.ini` from the same folder as the executable. If the
file doesn't exist, it's created with defaults on first run. A documented
template is provided in [`VideoSync.ini.sample`](VideoSync.ini.sample).

```ini
[General]
pollIntervalSeconds=60     ; periodic re-scan interval
debounceSeconds=3          ; wait after last change before syncing

[Logging]
enabled=false              ; set true to enable file logging
file=VideoSync.log         ; relative to the exe, or an absolute path

; Every section other than [General] and [Logging] is a sync pair.
; The section name is just a label.
[Movies]
source=D:\Torrents\Video\Movies
destination=\\Nas\data\Video\Movies

[TVShows]
source=D:\Torrents\Video\TVShows
destination=\\Nas\data\Video\TVShows
```

Add as many sync-pair sections as you like. INI format means Windows paths need
no backslash escaping.

## Usage

1. Build (or copy the deployed folder somewhere convenient).
2. Edit `VideoSync.ini` with your source/destination folders.
3. Run `VideoSync.exe` — it appears in the system tray.
4. When files appear in a source folder, they're moved to the destination
   automatically. Use **Sync Now** to force a pass, or **Exit** to quit.

## Logging

Logging is optional and disabled by default. Set `enabled=true` under
`[Logging]` to append timestamped activity (moves, warnings, errors) to the log
file. Console/debugger output is unaffected.

## Caveats

- **Files are moved, not copied** — the source is emptied as files transfer. If
  a destination file already exists, it is overwritten.
- Make sure the destination (e.g. the NAS share) is reachable; unavailable
  destinations are logged as warnings.
- Windows-only.

## Project layout

| Path | Purpose |
| --- | --- |
| `main.cpp` | All logic: config, logging, watcher, debounce, recursive move |
| `mainwindow.{h,cpp}` | Unused stub (no main window) |
| `resources.qrc`, `icons/` | Tray icons (blue = idle, green = syncing) |
| `CMakeLists.txt` | Build configuration |
| `build.bat` | Build a release + `windeployqt` into `build\release\` |
| `box.bat` | One-click: build, then package into a single boxed exe |
| `make_box.ps1` | Generates `VideoSync.evb` and runs Enigma Virtual Box |
| `VideoSync.evb` | Generated Enigma Virtual Box project (boxing input) |
| `VideoSync.ini.sample` | Documented configuration template |
