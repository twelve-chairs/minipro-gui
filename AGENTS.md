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

- `MainWindowTest` never calls the real CLI. It copies `tests/fake_minipro.sh` into a temp dir as `minipro`, puts it first on `PATH`, and steers it with `FAKE_MINIPRO_*` env vars (see the script header). Every call's arguments are logged, so tests can check the exact command line. The fake's output is shaped to match MainWindow's regex parsing; it is not a capture from real hardware. Each test runs inside a temp working dir, and reads go to the window's own `read_file_path()`.
- Tests reach private members through `friend class MainWindowTest` in `MainWindow.h`.
- Record a known, unfixed bug with `QEXPECT_FAIL`, or with `QSKIP` if it would crash the run. When the bug is fixed, remove the marker: an XPASS counts as a failure.
- Write and Update Firmware open a modal `QFileDialog`, so they have no tests.

CI (`.github/workflows/tests.yaml`) runs `cd tests && ./run_tests.sh` on macOS. The Ubuntu and macOS build workflows only compile.

## Architecture

- `src/MainWindow.{h,cpp}` — everything except the hex model. The constructor builds the whole UI programmatically (no `.ui` files) inside a separate top-level `window` (`std::unique_ptr<QWidget>` owned by `MainWindow`; the `QMainWindow` itself is never shown). Signals are wired with old-style `SIGNAL()/SLOT()` string macros.
- Two ways of invoking `minipro`:
  - `run_process()` — **synchronous**, for quick queries only: `--version` → `--presence_check` → `-l` (device list) → `-d <device>` (device info). It closes minipro's input, so the "Which database...?" prompt minipro shows without a programmer aborts at once. It stops minipro after `sync_timeout_ms` (10 s), and it reports failures in the Output pane rather than returning them as output. Output is regex-parsed: everything before `Serial code:` is stripped, `Name:`/`Memory:`/… fields are extracted, and `\e[K` escape sequences are removed. Anything that touches the chip goes through `run_async_process()`, including Run Command.
  - `run_async_process()` — **asynchronous** `QProcess` for device operations (read `-r`, write `-w`, firmware update `-F`, and Run Command's erase or checks), streaming stderr/stdout into the Output pane. `async_process_finished()` loads the hex view only after a **Read** that exited normally with code 0. A failed read, a crash, a non-zero exit, or any write or firmware update resets it to the placeholder, and every failure is reported in the Output pane (`async_process_error()` covers `FailedToStart`). `read_device()` deletes the previous read file before it starts, so an old file is never shown as new data. If the read file can't be opened after a successful read, that's reported as an error, not shown as an empty chip.
- **One device operation at a time.** `async_process` is the single running operation, parented to the window, and is deleted with `deleteLater()` in `end_async_process()`. `update_controls()` decides every control's state from the current state (operation running, `minipro_found`, `programmer_found`, `device_selected()`); call it after any of those change. While an operation runs it disables the action buttons and both dropdowns, since selecting a device or programmer also calls minipro. `refuse_if_operation_running()` is the backstop in every entry point. Each operation's input is closed right after `start()`, so minipro's stdin prompts (like choosing a database when no programmer is attached) read end-of-input and abort instead of hanging. The exception is a firmware update. Its input stays open until `answer_firmware_prompt_if_asked()` sees one of the prompts in `firmware_prompts`. That's the `y/n` question on TL866II+/T48/T56/T76, or the TL866A/CS menu, where Yes sends `1` (reflash its own variant) and No sends `3`; switching between A and CS (`2`) is never sent. It then asks `confirmations.firmware_update`, a Yes/No dialog defaulting to No that shows only the version lines minipro printed before its question, and writes the answer. A new minipro prompt must be added to `firmware_prompts`, or the update waits until the app closes. Write, and Run Command with Erase, first ask `confirmations.device_change` (a Yes/No warning naming the device and file, defaulting to No); checks don't ask. Tests replace both confirmation functions so no dialog opens: `makeWindow()` declines firmware updates and accepts writes and erases. `~MainWindow()` waits for a running operation instead of killing minipro mid-write.
- Checkboxes come in two kinds. **Options** (`parse_option_checkboxes()`: `--no_id_error`, `--skip_id`, `--no_size_error`, `--skip_verify`, `--pin_check`) are added to Read, Write and Run Command. **Actions** (`parse_action_checkboxes()`: `--blank_check`, `--erase`, `--hardware_check`) are separate minipro operations, so only Run Command uses them, and it requires exactly one. Never add an action to another operation: minipro lets `--blank_check` replace `-r`/`-w` and runs `--hardware_check` as soon as it's parsed. Update Firmware (`-F`) gets no checkbox flags, because minipro handles it before any flag that follows.
- `MainWindow`'s widgets are grouped by panel in small structs (`targets`, `buttons`, `device_info`, `options`, `actions`); labels and layouts are locals in the constructor. Keep new widgets in these groups rather than adding top-level fields: Sonar's `cpp:S1820` flags classes with more than 20.
- Startup flow gates the UI. `check_for_minipro()` sets `minipro_found` and the window title from the version line. `check_for_programmer()` sets `programmer_found`. `update_controls()` then enables Update Firmware for any found programmer, but Read, Write and Run Command only while `device_selected()`: a non-empty `device` from the `-l` list. Those operations also refuse an empty device themselves (`refuse_if_no_device()`).
- Device reads go to `read_file_path()`: `read.bin` in a private (0700) `QTemporaryDir`, created with the window and removed with it. They never go to the working directory: apps started from the Finder run in `/`, and a user's own file there could be overwritten. After a successful read, `build_formatted_hex_output()` passes the file's bytes to the model.
- `src/HexView.{h,cpp}` — `QAbstractTableModel` for the Hex Viewer: 16 byte columns plus an ASCII column. It keeps the raw `QByteArray` (`loadBytes()`, or `buildHexTable()` from a hex string) and formats each cell in `data()` only when the view asks for it. This keeps memory close to the image size; a 16 MiB chip has a million rows. The column count is fixed at 17, a short last row has blank cells, and `clearHexTable()` shows 8 placeholder rows of `--`. `format_hex_table_columns()` gives every row the same fixed height: sizing rows to their contents takes minutes on large images.

## Workflow

- Commit with `/commit` (`.claude/skills/commit/`). It builds the app, runs `tests/run_tests.sh`, scans for secrets, reviews the diff against `.claude/rules/security-and-privacy.md`, updates README.md and AGENTS.md where the change made them wrong, commits per `.claude/rules/commit-messages.md`, and pushes.
- `master` is the default branch. Work on a feature branch and open a pull request; don't commit to `master` directly.
- **Never run the real `minipro` against a connected programmer as a test.** Writes (`-w`), erases (`--erase`) and firmware updates (`-F`) change the chip or the programmer and can't be undone. Automated tests use `tests/fake_minipro.sh`. Running the real CLI, and above all anything that writes, needs the user to ask for it.
- GitHub Actions: on pushes and pull requests to `master`, the Unit Tests workflow runs when `src/` or `tests/` changes, and the Ubuntu and macOS builds run when `src/` changes. All three also run monthly and can be started by hand. They run with a read-only token (`permissions: contents: read`), and actions are pinned to commit SHAs with the version in a comment. Keep both when editing a workflow. CodeQL scans the code on its own schedule. Treat a failing workflow or a new CodeQL finding like a failing test.
- SonarQube Cloud analyses the repo as `twelve-chairs_minipro-gui`, and the README shows its quality gate. Treat a failing gate like a failing test. The `sonarqube` MCP tools read its issues, hotspots and gate; `/security-audit` uses them.
- Audit the whole repo with `/security-audit` (`.claude/skills/security-audit/`) before a release, and after a change to how `minipro` is invoked, its output parsed, or files read or written.
- The repository is public: no secrets, signing certificates or real people's data.
