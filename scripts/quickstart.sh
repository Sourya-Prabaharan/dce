#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
# Use a new directory to verify a clean build without deleting existing builds.
BUILD_DIR=${BUILD_DIR:-build-quickstart}
sh scripts/deps.sh
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j "${BUILD_JOBS:-4}"
ctest --test-dir "$BUILD_DIR" --output-on-failure
"$BUILD_DIR/dce-cli" generate --output data/numbers.csv --rows 10000
"$BUILD_DIR/dce-verify" "$BUILD_DIR" . quickstart
