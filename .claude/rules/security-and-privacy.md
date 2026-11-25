# Security and Privacy Rule

minipro-gui is a local desktop app. It runs the `minipro` CLI on the user's
machine, which reads, writes and erases chips and can reflash the programmer's
firmware. It has no server, accounts or online services. The repository is
**public**. Every change is checked against this list before it is committed,
and the `/commit` skill reviews the diff against it. **A violation blocks the
commit.** Fix it; don't note it for later.

## Secrets and repository hygiene

- No API keys, tokens, passwords, private keys, signing certificates (`.p12`,
  `.cer`, `.mobileprovision`) or notarization credentials in code, config or
  history. gitleaks scans changes on every commit and the full history in
  `/security-audit`.
- The Apple Development Team ID in `CMakeLists.txt` is a public identifier, not
  a secret. Signing identities and app-specific passwords are secrets: they
  stay in the keychain or CI secrets.
- If a secret is ever committed, treat it as leaked: revoke and rotate it
  first. Rewriting history doesn't make it secret again, because the repo is
  public.
- Never commit `.claude/settings.local.json`, `.env*`, build directories
  (`cmake-build-*/`, `tests/build/`), `.idea/workspace.xml` or chip images read
  from a user's device (`temp.bin`, `*.bin`). Chip contents can be proprietary
  firmware. `.claude/settings.json` holds no absolute local paths or tokens.

## Running minipro

All device access goes through `QProcess`. The rules here prevent command
injection and stop the app damaging hardware by accident.

- Start `minipro` by the absolute path `minipro_path()` finds (on `PATH`, then
  in fixed install folders such as Homebrew's), with
  a `QStringList` of arguments (`process->start(minipro_path(), args)`). Never
  start it by bare name: Windows would search the current directory first, so
  a planted `minipro.exe` there would run. **Never** go through a shell: no
  `system()`, `popen()`, `sh -c`, `QProcess::startCommand()`, and no
  single-string command lines. Device names, file paths and options are
  separate list items, never spliced into a string.
- Device names come from the `minipro -l` list, and file paths from
  `QFileDialog`. Neither may contain anything the app didn't pass through
  unchanged as one argument. A value starting with `-` must not be able to turn
  into an option: check it against the device list, or put it after the flag
  that takes it.
- Operations that change the chip or programmer (`-w`, `--erase`, `-F`) run
  only from an explicit user action: a button the user pressed, followed by a
  file they chose. Never start one automatically, on startup, from a signal
  like `highlighted`, or by default.
- Tests never run the real CLI. They use the fake `minipro` (`tests/fake_minipro.cpp`) on `PATH`,
  because a real programmer could be connected to the machine running them.

## Untrusted input

Treat everything the app did not compute itself as untrusted:

- **minipro output** (stdout and stderr). Its format changes between versions
  and with the programmer attached. Parsing must never crash, index out of
  range or loop forever on unexpected text: empty output, a missing field, a
  very long line, or escape sequences other than `\e[K`. When parsing fails,
  show the raw output in the Output pane and keep going.
- **Files read from disk** (the read result, files chosen for writing). Check that
  `QFile::open` succeeds and handle failure. Handle any size: empty, under one
  row, and multi-megabyte. A malformed file shows an error; it never crashes
  the app.
- **Process exit codes and crashes.** `QProcess::FailedToStart`, a crash, and
  non-zero exit codes each reach the user as a message, not as stale or partial
  data presented as a successful read.

## Files the app writes

- Device reads go to `read.bin` in the window's private (0700) `QTemporaryDir`
  (`read_file`), which is removed when the app exits. Any other file the
  app writes goes to a path the user chose or to a private `QTemporaryDir` /
  `QTemporaryFile`. Never write to a fixed name in the working directory or a
  shared folder, where it could overwrite a user's file or a symlink could
  redirect it.
- Never delete or overwrite a file the user didn't choose for that operation.

## No network, no tracking

The app makes no network requests. Adding any of the following is a design
change for the user to decide, not something to slip into a code review:

- `QNetworkAccessManager`, sockets, update checks, crash reporting, analytics
  or telemetry.
- Sending device names, chip contents or file paths anywhere.

## Dependencies and CI

- The runtime dependencies are Qt 6 (Core, Gui, Widgets) and the `minipro` CLI,
  both installed by the user. Adding another one needs a reason in the commit
  message.
- GitHub Actions run with least-privilege `permissions:` (the workflows set
  `contents: read`), never use `pull_request_target` to check out PR code, and
  never echo secrets. Every action is pinned to a commit SHA, with its version
  in a comment (`actions/checkout@<sha> # v7.0.1`).
