#!/bin/sh
# Build and run all unit tests. Extra arguments are passed to ctest (e.g. -R hexview -V).
set -e
cd "$(dirname "$0")"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure "$@"
