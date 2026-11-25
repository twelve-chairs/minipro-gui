# Commit Message Rule

Every git commit must have a meaningful message. Follow these requirements:

## Format

```
<type>: <short summary> (≤72 chars)

- <what changed and why, one bullet per logical change>
- <additional context if non-obvious>
```

## Types

| Type | Use when |
|------|----------|
| `feat` | New user-facing functionality (a new minipro option, a new view) |
| `fix` | Bug fix |
| `refactor` | Code restructure with no behavior change |
| `ui` | Layout, widgets or styling only |
| `test` | Tests or the fake `minipro` only |
| `docs` | README, AGENTS.md, CLAUDE.md, skills, rules |
| `build` | CMake, Qt version, compiler settings, signing config |
| `ci` | GitHub Actions workflows |
| `chore` | Housekeeping with no build or behavior impact |
| `security` | Security or privacy-related changes |

A change that fixes a bug and adds the test that catches it is `fix`, not `test`.

## Areas

Name the affected area in the summary where it helps scanning: `hexview` (hex
table model), `mainwindow` (UI and layout), `cli` (how `minipro` is invoked and
its output parsed), `device` (read, write, erase, firmware update), `tests`,
`ci`, `build`.

Example: `fix(hexview): keep a trailing row shorter than 16 bytes`

## Rules

- **Never** use vague messages like `"fix"`, `"update"`, `"wip"`, or `"changes"`.
- The summary line must describe **what** changed, not just **where**.
- If multiple files changed, list each logical change as a bullet in the body.
- When a change alters what is passed to `minipro`, give the arguments before
  and after (`-p <dev> -r temp.bin` → `-p <dev> -r <path>`).
- State the *reason* for anything non-obvious, especially workarounds for
  `minipro` output quirks. Otherwise someone who can't see why they exist
  "cleans them up" later.
- Say the tests pass only if they were run for this commit and passed.

## Examples

Good:

```
fix(hexview): stop the crash when fewer than 16 bytes are loaded

- columnCount() read row 0 of an empty table; it now returns a fixed 17
- Reads of an empty or missing temp.bin now show an empty table
- Adds readWithNoDataShowsEmptyTable, which crashed on the old code
```

```
feat(device): add a Verify button that runs minipro -m

- Compares the chip against a chosen file without writing to it
- Uses the async path so the Output pane streams progress
```

Bad:

```
update files
fix bug
wip
```
