---
name: commit
description: Build the app and run the unit tests (fixing the code if they fail), scan for secrets and review the diff against the security rules, update README.md and AGENTS.md if relevant, commit all staged/unstaged changes on a feature branch, and push to GitHub.
argument-hint: [optional commit message override]
---

Follow these steps in order.

## 1. Assess changes

Run `git status`, `git branch --show-current` and `git diff HEAD` to see what
changed, including untracked files.

If the current branch is `master`, create a feature branch first
(`git switch -c <short-descriptive-name>`). Never commit to `master` directly.

## 2. Update README.md

**Check `README.md` on every commit.** It is the first thing people see on the
public repo, and it goes stale without anyone noticing. Update anything the
change has made wrong or incomplete:

- Dependencies and install commands for each OS (macOS, Ubuntu/Debian, Fedora,
  openSUSE)
- The build and run commands
- What the app can do, if a feature was added or removed
- Badges and links that now point at something renamed or deleted

If nothing in the README is affected, say so explicitly in your report rather
than skipping the step silently.

Keep each file to its job. The README says **what the app is and how to build
and run it**. `AGENTS.md` says **how agents work in this repo**: commands, test
setup, architecture, workflow. Link rather than copy.

## 3. Update AGENTS.md if relevant

Update it when something agents rely on changed: a build or test command, the
test setup (new test executable, new `FAKE_MINIPRO_*` variable), a new rule or
skill, a workflow trigger, or an architecture fact it states (for example how
`minipro` is invoked or where reads are written). Re-read the Architecture section
against the diff; it goes stale fastest.

## 4. Clean .claude/settings.json

If `.claude/settings.json` exists, move any `permissions.allow` entries that
contain absolute local paths into `.claude/settings.local.json`. That means
anything matching `/Users/`, `/home/` or `/private/`, or a `/tmp/` reference
with a specific filename (not a wildcard pattern like `Bash(cmake:*)`). Merge
with the existing entries without duplicating any. Save both files if anything
changed. `.claude/settings.local.json` is gitignored; never stage it.

## 5. Build and test

Run, in order:

1. Build the app:

   ```bash
   cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja -S . -B ./cmake-build-debug
   cmake --build ./cmake-build-debug --target minipro-gui
   ```

   It must build. Report every compiler warning in a changed file. A new
   warning introduced by this change is fixed before committing. Existing
   warnings are listed, not fixed, unless the change touches that line.
2. `tests/run_tests.sh` must pass, with no unexpected `XPASS` and no new `SKIP`
   or `QEXPECT_FAIL` added without a known-bug explanation.
3. If the change can alter what the window looks like (anything in `src/`
   except `HexView` internals that don't change what's shown, or
   `res/AppIcon.*`), run `tests/update_screenshot.sh` on macOS. It re-renders
   `res/screenshot.png`, the README screenshot, from the real UI with the fake
   `minipro`, and gives the same image for the same UI. If `git status` shows
   the PNG changed, look at it (Read the file) before staging it with the
   commit; if it looks wrong, that's a bug in the change. On other systems,
   skip this and say so: the README keeps the macOS look.

Skip all three only for a change confined to Markdown or `.claude/`, and say so.

Do not commit if any of them fails.

**If a test fails, fix the code, not the test.** Read the failure, find the
cause, fix it, and run again until it passes. Change a test only when this
commit deliberately changes the behavior it checks, and say so in the report and
the commit message. Never delete, skip or weaken a test to get a commit through.
If the fix needs a product decision, stop and ask the user.

**Then check the tests cover the change.** New behavior gets tests. A bug fix
gets the test that would have caught it. A change to how `minipro` output is
parsed gets a case in `tests/fake_minipro.cpp` and a test for malformed output.
A new argument passed to `minipro` gets a test that checks the logged command
line.

Never run the real `minipro` to test a change unless the user asks: a
connected programmer could write to or erase a chip.

Report the result: how many tests ran and passed, and which were added or
changed and why.

## 6. Security review

1. Scan for secrets in the changes and the tracked tree:

   ```bash
   gitleaks git --pre-commit --no-banner --redact -v
   gitleaks dir --no-banner --redact -v --max-target-megabytes 5 src tests res .github .claude CMakeLists.txt
   ```

2. Read the full diff against `.claude/rules/security-and-privacy.md`. Grep the
   changed files for the sinks it names:

   ```bash
   git diff HEAD --name-only | grep -E '\.(cpp|h|sh|txt|yaml|yml)$' | xargs -r grep -nE \
     'system\(|popen\(|startCommand|startDetached|"sh"|"/bin/sh"|"-c"|QNetworkAccessManager|QTcpSocket|QUdpSocket|pull_request_target|"--erase"|"-w"|"-F"'
   ```

   Each hit is either fine or a violation. `-w`, `-F` and `--erase` are fine
   only inside a slot the user triggers by hand. A `"-c"` is fine when it isn't
   a shell flag.
3. If `.github/workflows/` changed, check the permissions and how actions are
   pinned against the rule.

Then decide:

- **Any security violation blocks the commit**: a secret or signing
  credential, a shell-built command line, an argument that could become an
  option, a destructive operation without an explicit user action, a network
  call, unchecked parsing of `minipro` output or files, or a tracked chip image.
  Fix it, or stop and ask the user if the fix is a product decision.
- For a large or risky change (process invocation, output parsing, file
  handling, anything that writes to a device), also run `/code-review` on the
  diff and act on what it finds.

Report the result even when clean: "Security: no secrets, no rule violations",
or the list of what was fixed.

## 7. Commit

Stage deliberately with `git add <paths>` after reading `git status`. Never use
`git add -A` blindly. Never stage `.claude/settings.local.json`, `.env*`,
`cmake-build-*/`, `tests/build/`, `*.bin`, `.DS_Store` or
`.idea/workspace.xml`.

Write the message following `.claude/rules/commit-messages.md`. If the user
supplied `$ARGUMENTS`, use that as the summary line (still applying the correct
type prefix).

Append a co-author trailer naming the model that actually wrote the commit.
Substitute your own model name; don't copy the example verbatim:

```
Co-Authored-By: Claude <your model name> <noreply@anthropic.com>
```

For example: `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`. A
hardcoded name here would go stale and credit a model that didn't do the work.

Never pass `--no-verify`.

## 8. Push

Run `git push` after a successful commit. The remote is
`https://github.com/twelve-chairs/minipro-gui.git` (public) and the default
branch is `master`. If the branch has no upstream yet, use
`git push -u origin <branch>`. Don't open a pull request unless the user asks.

## 9. Report

Summarize: the branch, the commit hash and summary line, README and AGENTS.md
(updated or not, and why), the build and test results (step 5) and the security
review (step 6).
