---
name: security-audit
description: Comprehensive, repo-wide security audit of minipro-gui, not just a diff. Checks secrets in the full git history, CI and supply chain, how the minipro CLI is invoked (command injection, option injection, destructive operations without a user action), parsing of minipro output and files, file handling, network use, the build and signing setup, and SonarQube Cloud's open findings and ratings. Read-only; reports findings by severity and fixes nothing without asking. For reviewing only the pending diff, use /security-review instead.
argument-hint: [source | ci | build | sonar | full]
---

`/commit` checks each **diff** against `.claude/rules/security-and-privacy.md`.
This skill checks **everything that already exists**: code that predates a
rule, history that was never scanned, and what actually ships. Run it before a
release, after a change to how `minipro` is invoked or its output parsed, after
adding a dependency or workflow, and whenever the rules change.

**Arguments.** `source` runs sections 1, 3–6. `ci` runs section 2. `build` runs
section 7. `sonar` runs section 8. `full` or nothing runs everything.

**This is an audit, not a fix.** Don't edit, commit or push anything while it
runs. Report first (section 9). The user decides what to fix, and each fix goes
through `/commit` like any other change. The standard is
`.claude/rules/security-and-privacy.md`.

For every check, **record a result even when it is clean**. A check with no
recorded result can't be told apart from one that was skipped.

**Never run the real `minipro` during the audit.** A programmer may be
connected. Reason from the code and the fake (`tests/fake_minipro.sh`).

## 0. Preconditions

```bash
command -v gitleaks
git status --short
git branch --show-current
git rev-parse --short HEAD
gh repo view --json visibility
```

- If gitleaks is missing, install it (`brew install gitleaks`). Don't skip its
  checks.
- Run `tests/run_tests.sh`. A failing test is a finding at the severity of what
  it checks.
- Note the commit audited and any uncommitted changes, so the report says what
  was audited.

Read all of `src/`; it is small. The greps below find candidates, and reading
the code confirms them.

## 1. Secrets and repo hygiene

```bash
gitleaks git --no-banner --redact -v          # the entire history
gitleaks dir --no-banner --redact -v .        # untracked files too
git ls-files | grep -Ei '\.env|\.pem|\.key|\.p12|\.cer|\.mobileprovision|secret|credential|settings\.local\.json|\.bin$'
git log --all --diff-filter=A --name-only --format= | sort -u | grep -Ei '\.env|\.pem|\.key|\.p12|\.mobileprovision|settings\.local|\.bin$'
```

- Any secret or signing credential in history is **Critical**, even if since
  deleted, because the repo is public. The remedy is revoke and rotate, not a
  history rewrite.
- A chip image (`*.bin`) in history is **High** if it could be someone's
  proprietary firmware, otherwise **Low**.
- `.gitignore` still covers everything the rule says must never be committed.
- `.claude/settings.json` (if tracked) holds no tokens or absolute local paths.

## 2. CI and supply chain

Read every file in `.github/workflows/`.

- `permissions:` missing or broader than needed: **Low**. Write access with an
  untrusted trigger: **High**.
- `pull_request_target` or `workflow_run` that checks out or runs PR code:
  **High**.
- Actions pinned to a tag, not a commit SHA: **Low**, with each `uses:` line
  listed.
- Secrets echoed or passed to untrusted steps: **High**.
- Tools installed unpinned (`brew install qt minipro`): **Info**, since this is
  normal for Homebrew. Note the risk.
- Malformed trigger blocks (for example a `workflow_run:` indented under
  `workflow_dispatch:`): **Info**, as correctness.

## 3. Running minipro

```bash
grep -rnE 'QProcess|start\(|startDetached|startCommand|system\(|popen\(|"sh"|"-c"' src
```

For every call site:

- **Command injection.** The program and its arguments are passed separately,
  never through a shell or as a single command string. A shell-built command
  line from a device name or file path is **Critical**.
- **Option injection.** Trace each argument back to its source: the device
  list, `QFileDialog`, checkboxes, constants. If a value can start with `-` and
  be read as a `minipro` option (for example a chosen file named
  `--erase.bin`), that is **Medium**, or **High** if it can trigger a write or
  erase.
- **Destructive operations.** `-w`, `--erase` and `-F` are reachable only from
  a slot the user triggered by hand, after choosing a file. Any automatic path
  (startup, `highlighted`, `currentTextChanged`, a default) is **High**. Check
  the signal connections in the constructor, not just the slots.
- **Empty or stale arguments.** A device operation sent with an empty device
  name, or the previous device after the list changed: **Medium**.
- **Process lifecycle.** A `QProcess` leaked per call, a second async process
  started while one runs and sharing `async_process`, blocking waits on the UI
  thread that can hang the app: **Low** (robustness), but list each one.

## 4. Untrusted input

- **minipro output.** For every regex and split in `run_process`,
  `check_for_minipro`, `check_for_programmer`, `get_devices` and
  `select_device`, try these against the code: empty output, the field
  missing, extra lines, Windows line endings, a very long line, unexpected
  escape sequences, and output from a newer `minipro` version. A crash or out-of-range
  index is **Medium**. Wrong data shown as if it were correct (for example
  another device's info) is **Low**.
- **Files.** `QFile::open` results are checked, and empty, tiny and huge files
  are handled. An unchecked open, a crash or unbounded memory use is
  **Medium**.
- **Exit status.** Failure, a crash or `FailedToStart` shown as success, or a
  hex view showing a stale `temp.bin` after a failed read: **Medium**.
- Tests exist for the malformed cases. A missing test for a parser is **Low**.

## 5. Files the app writes

```bash
grep -rnE 'QFile|temp_file|\.bin|QTemporary|QStandardPaths|remove\(|rename\(' src
```

- `temp.bin` is written to the current working directory. When the app is
  started from a shared or world-writable directory, a symlink planted there
  could redirect the write, and the next read could show attacker-chosen data.
  Report this as **Low** with that scenario, or higher if the code writes
  anywhere else.
- Any file deleted or overwritten that the user didn't choose: **High**.

## 6. Network and tracking

```bash
grep -rnE 'QNetwork|QTcpSocket|QUdpSocket|QWebSocket|https?://' src CMakeLists.txt
grep -n 'Network' CMakeLists.txt tests/CMakeLists.txt
```

- Any network use the rule doesn't allow: **High**.

## 7. Build and signing

```bash
grep -nE 'XCODE_ATTRIBUTE|CODE_SIGN|TEAM|TIMESTAMP|OTHER_CODE_SIGN' CMakeLists.txt
```

- Signing config references a keychain identity, not a file or password in the
  repo. A password or certificate path: **High**.
- Hardened runtime (`--options=runtime`) stays enabled for signed builds.
  Removing it is **Medium**.
- Build output or `.app` bundles tracked in git: **Low**.

## 8. SonarQube Cloud

SonarQube Cloud analyses the repo as project `twelve-chairs_minipro-gui`, and
the README shows its quality gate. Read it through the `sonarqube` MCP tools
(`mcp__sonarqube__*`). If they aren't connected, record "not checked: SonarQube
tools unavailable" and say how to connect them (`/sonar-integrate`).

- **Ratings and gate.** Call `get_component_measures` for `security_rating`,
  `reliability_rating`, `sqale_rating`, `security_hotspots_reviewed`, `bugs`,
  `vulnerabilities` and `code_smells`, then call
  `get_project_quality_gate_status`. Ratings come back as numbers: 1 is A, 5 is E.
- **Open issues.** Call `search_sonar_issues_in_projects` with
  `issueStatuses: ['OPEN', 'CONFIRMED']`, once for each software quality
  (`SECURITY`, `RELIABILITY`, `MAINTAINABILITY`). Read the code at each issue,
  and `show_rule` for an unfamiliar rule, before judging it.
- **Security hotspots.** Call `search_security_hotspots` with
  `status: 'TO_REVIEW'`. Each needs a verdict: safe (with the reason) or a
  finding.

Grade each one as a finding of this audit, not by Sonar's own severity:

- A vulnerability, or an unreviewed hotspot that is real under the rule, takes
  the rule's severity.
- A security or reliability rating below A is at least **Low**, with the issues
  that cause it listed, since it shows on the public README.
- Reliability and maintainability issues with no security impact are **Info**,
  grouped by rule, each with a recommended fix: change the code, or mark it as
  a false positive or accepted in Sonar with the reason. Marking needs the
  user's approval, since it changes a shared record.
- List a finding the local checks already reported once, with both sources.

## 9. Report

Present the report in chat in this structure:

```
# Security Audit — <date> — <commit> — scope: <scope>

## Summary
<2–4 sentences: overall posture, count by severity, the single most important fix>

## Findings
### [CRITICAL|HIGH|MEDIUM|LOW|INFO] <short title>
- **Where:** `path/to/file.cpp:line`
- **Rule:** <section of .claude/rules/security-and-privacy.md>
- **Issue:** <what is wrong>
- **Scenario:** <concrete: what input or situation, what goes wrong — data loss, a bricked chip, a crash>
- **Fix:** <specific change>

## Checked and passing
<every check from sections 0–8 with its result>

## Not checked
<anything skipped and why>
```

Severity guide:

- **Critical**: a secret or signing credential in history, or shell command
  injection.
- **High**: a write, erase or firmware update that can start without an
  explicit user action, option injection that reaches a destructive operation,
  network use, a risky CI trigger, or proprietary chip images in the repo.
- **Medium**: a crash or wrong-device operation from unexpected `minipro`
  output or files, failure shown as success, or other option injection.
- **Low**: defense-in-depth gaps, unpinned actions, missing parser tests,
  robustness issues with processes.
- **Info**: hardening suggestions and design notes.

Only report findings you verified by reading the code or command output. If
you're unsure, put it under Info with what would confirm it.

Finally, offer to (a) publish the report as a private artifact for sharing, and
(b) fix findings in severity order, each as its own `security:` commit with
tests, through `/commit`.
