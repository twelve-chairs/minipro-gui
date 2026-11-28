#!/bin/sh
# Build the unit tests with clang source-based coverage, run them, and write the report SonarQube
# Cloud reads (sonar-project.properties): build-coverage/coverage.txt, in `llvm-cov show` format,
# and build-coverage/compile_commands.json. Extra arguments are passed to ctest.
# Prints a per-file summary at the end.
set -e
cd "$(dirname "$0")"
build=build-coverage

if command -v xcrun > /dev/null 2>&1; then
  profdata="xcrun llvm-profdata"
  cov="xcrun llvm-cov"
else
  profdata=llvm-profdata
  cov=llvm-cov
fi

cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build "$build"

# Profiles from an earlier run would be merged into this one's.
rm -rf "$build/profiles"
mkdir -p "$build/profiles"
# %p: one file per process, so the two test executables don't overwrite each other's.
LLVM_PROFILE_FILE="$PWD/$build/profiles/%p.profraw" \
  ctest --test-dir "$build" --output-on-failure "$@"

$profdata merge -sparse -o "$build/merged.profdata" "$build"/profiles/*.profraw
# Report only the app's own code, not Qt's headers, moc output or the tests.
filter='(/tests/|/build-coverage/|/Qt|/qt|/usr/)'
$cov show --show-branches=count --instr-profile "$build/merged.profdata" \
  "$build/hexview_test" -object "$build/mainwindow_test" \
  --ignore-filename-regex="$filter" > "$build/coverage.txt"
$cov report --instr-profile "$build/merged.profdata" \
  "$build/hexview_test" -object "$build/mainwindow_test" \
  --ignore-filename-regex="$filter"
