#!/bin/sh
# Foreground local cluster. Ctrl-C stops only the processes started by this script.
set -eu
cd "$(dirname "$0")/.."
BUILD_DIR=${BUILD_DIR:-build}
WORKERS=${WORKERS:-3}
THREADS=${THREADS:-1}
mkdir -p run
pids=""
cleanup() { trap - EXIT INT TERM; for pid in $pids; do kill -TERM "$pid" 2>/dev/null || true; done; for pid in $pids; do wait "$pid" 2>/dev/null || true; done; }
trap cleanup EXIT INT TERM
"$BUILD_DIR/dce-coordinator" --bind "${BIND:-127.0.0.1}" --data data --dashboard web/index.html >run/coordinator.log 2>&1 &
pids="$!"
tries=0
until "$BUILD_DIR/dce-cli" health >/dev/null 2>&1; do
  tries=$((tries+1)); [ "$tries" -lt 50 ] || { echo 'Coordinator did not start; inspect run/coordinator.log'; exit 1; }; sleep 0.1
done
i=1
while [ "$i" -le "$WORKERS" ]; do
  "$BUILD_DIR/dce-worker" --id "worker-$i" --threads "$THREADS" --data data >"run/worker-$i.log" 2>&1 &
  pid=$!; pids="$pids $pid"; echo "$pid" >"run/worker-$i.pid"
  i=$((i+1))
done
echo "Dashboard: http://127.0.0.1:8080 · $WORKERS workers × $THREADS threads. Ctrl-C to stop."
wait
