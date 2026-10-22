#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p results
{ date -u; uname -a; c++ --version; if command -v sysctl >/dev/null; then sysctl machdep.cpu.brand_string hw.ncpu hw.memsize; else lscpu; fi; } >results/hardware.txt 2>&1
"${BUILD_DIR:-build}/dce-verify" "${BUILD_DIR:-build}" . benchmark
