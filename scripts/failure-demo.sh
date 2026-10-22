#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p results
"${BUILD_DIR:-build}/dce-verify" "${BUILD_DIR:-build}" . failure
