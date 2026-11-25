# minipro-gui

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
  - `run_async_process()` — **asynchronous** `QProcess` for long operations (read `-r`, write `-w`, firmware update `-F`), streaming stderr/stdout into the Output pane; `finished` → `read_device_output()`.
- Checkboxes come in two kinds. **Options** (`parse_option_checkboxes()`: `--no_id_error`, `--skip_id`, `--no_size_error`, `--skip_verify`, `--pin_check`) are added to Read, Write and Run Command. **Actions** (`parse_action_checkboxes()`: `--blank_check`, `--erase`, `--hardware_check`) are separate minipro operations, so only Run Command uses them, and it requires exactly one. Never add an action to another operation: minipro lets `--blank_check` replace `-r`/`-w` and runs `--hardware_check` as soon as it's parsed. Update Firmware (`-F`) gets no checkbox flags, because minipro handles it before any flag that follows.
- Startup flow gates the UI: `check_for_minipro()` sets `minipro_found` and window title from the version string; `check_for_programmer()` enables/disables buttons based on `programmer_found`.
- Device reads go to a relative `temp.bin` in the current working directory; after the async read finishes, `build_formatted_hex_output()` loads it as hex and passes it to the model.
- `src/HexView.{h,cpp}` — `QAbstractTableModel` for the Hex Viewer: 16 byte columns + an ASCII column, `buildHexTable()` takes the file as a hex string and makes one row per 16 bytes, padding a short last row with blank cells. The column count is fixed at 17, so an empty table is valid. `clearHexTable()` resets to 8 placeholder rows of `--`.

## Workflow

- Commit with `/commit` (`.claude/skills/commit/`). It builds the app, runs `tests/run_tests.sh`, scans for secrets, reviews the diff against `.claude/rules/security-and-privacy.md`, updates README.md and AGENTS.md where the change made them wrong, commits per `.claude/rules/commit-messages.md`, and pushes.
- `master` is the default branch. Work on a feature branch and open a pull request; don't commit to `master` directly.
- **Never run the real `minipro` against a connected programmer as a test.** Writes (`-w`), erases (`--erase`) and firmware updates (`-F`) change the chip or the programmer and can't be undone. Automated tests use `tests/fake_minipro.sh`. Running the real CLI, and above all anything that writes, needs the user to ask for it.
- GitHub Actions: on pushes and pull requests to `master`, the Unit Tests workflow runs when `src/` or `tests/` changes, and the Ubuntu and macOS builds run when `src/` changes. All three also run monthly and can be started by hand. CodeQL scans the code on its own schedule. Treat a failing workflow or a new CodeQL finding like a failing test.
- SonarQube Cloud analyses the repo as `twelve-chairs_minipro-gui`, and the README shows its quality gate. Treat a failing gate like a failing test. The `sonarqube` MCP tools read its issues, hotspots and gate; `/security-audit` uses them.
- Audit the whole repo with `/security-audit` (`.claude/skills/security-audit/`) before a release, and after a change to how `minipro` is invoked, its output parsed, or files read or written.
- The repository is public: no secrets, signing certificates or real people's data.
