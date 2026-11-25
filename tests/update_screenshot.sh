#!/bin/sh
# Re-render res/screenshot.png (the README screenshot) from the current UI, with the fake minipro.
# Runs on the native platform, so it opens the app window for a moment; run it on macOS so the
# screenshot keeps the same look.
set -e
cd "$(dirname "$0")"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug > /dev/null
cmake --build build --target screenshot
./build/screenshot ../res/screenshot.png
