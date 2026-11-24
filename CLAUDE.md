# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Qt6 / C++20 desktop GUI wrapper around the [`minipro`](https://gitlab.com/DavidGriffith/minipro) CLI for TL866-series chip programmers. The app does no USB/programmer I/O itself — every operation shells out to the `minipro` binary on `PATH` and parses its text output.

## Build & run

Dependencies (macOS): `brew install cmake ninja qt6 minipro`. Linux needs `qt6-base-dev` (or distro equivalent) plus `minipro` built from source; see README.md.

```sh
cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_MAKE_PROGRAM=ninja -G Ninja -S . -B ./cmake-build-minsizerel
cmake --build ./cmake-build-minsizerel --target minipro-gui -- -j 2
./cmake-build-minsizerel/minipro-gui          # on macOS: ./cmake-build-minsizerel/minipro-gui.app/Contents/MacOS/minipro-gui
```

The root `CMakeLists.txt` globs `src/*.cpp` / `src/*.h`, so new source files are picked up on re-configure. On Apple it builds a signed `MACOSX_BUNDLE`.

## Tests

`tests/` is a **separate CMake project** (not added via `add_subdirectory`). It builds one QtTest executable per test class, `hexview_test` and `mainwindow_test`, and registers both with CTest. Source files are listed explicitly in `tests/CMakeLists.txt`: a new `src/*.cpp` file has to be added there, and `main.cpp` is excluded because `QTEST_MAIN` provides `main`.

```sh
tests/run_tests.sh                    # configure + build into tests/build, run ctest (headless, QT_QPA_PLATFORM=offscreen)
tests/run_tests.sh -R hexview -V      # extra args go to ctest
QT_QPA_PLATFORM=offscreen tests/build/mainwindow_test readDeviceFillsHexView   # one test function (-functions lists them)
```

- `MainWindowTest` never calls the real CLI. It copies `tests/fake_minipro.sh` into a temp dir as `minipro`, puts it first on `PATH`, and steers it with `FAKE_MINIPRO_*` env vars (see the script header). Every call's arguments are logged, so tests can check the exact command line. The fake's output is shaped to match MainWindow's regex parsing; it is not a capture from real hardware. Each test runs inside a temp working dir because of `temp.bin`.
- Tests reach private members through `friend class MainWindowTest` / `friend struct MainWindowDeleter` in `MainWindow.h`.
- Record a known, unfixed bug with `QEXPECT_FAIL`, or with `QSKIP` if it would crash the run. When the bug is fixed, remove the marker: an XPASS counts as a failure.
- Write and Update Firmware open a modal `QFileDialog`, so they have no tests.

CI (`.github/workflows/tests.yaml`) runs `cd tests && ./run_tests.sh` on macOS. The Ubuntu and macOS build workflows only compile.

## Architecture

- `src/MainWindow.{h,cpp}` — everything except the hex model. The constructor builds the whole UI programmatically (no `.ui` files) inside a separate top-level `QWidget *window` (not the `QMainWindow` itself — `main.cpp` never calls `show()` on `MainWindow`). Signals are wired with old-style `SIGNAL()/SLOT()` string macros.
- Two ways of invoking `minipro`:
  - `run_process()` — **synchronous** (`waitForFinished`), used for startup/discovery: `--version` → `--presence_check` → `-l` (device list) → `-d <device>` (device info). Output is regex-parsed (e.g. stripping everything before `Serial code:`, extracting `Name:`/`Memory:`/… fields, removing `\e[K` escape sequences).
  - `run_async_process()` — **asynchronous** `QProcess` for long operations (read `-r`, write `-w`, firmware update `-F`), streaming stderr/stdout into the Output pane; `finished` → `read_device_output()`. It appends the option checkboxes (`parse_checkboxes()` → `--skip_verify`, `--erase`, etc.) to the arguments.
- Startup flow gates the UI: `check_for_minipro()` sets `minipro_found` and window title from the version string; `check_for_programmer()` enables/disables buttons based on `programmer_found`.
- Device reads go to a relative `temp.bin` in the current working directory; after the async read finishes, `build_formatted_hex_output()` loads it as hex and passes it to the model.
- `src/HexView.{h,cpp}` — `QAbstractTableModel` for the Hex Viewer: 16 byte columns + an ASCII column, rows built from a hex string (2 chars per byte, 32 chars per row). `clearHexTable()` resets to 8 placeholder rows of `--`.

## Code quality

The project is tracked on SonarCloud (`twelve-chairs_minipro-gui`) and CodeQL; recent commits are Sonar-issue cleanups, so keep changes Sonar-clean.
