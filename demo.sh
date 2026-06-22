#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

MASTER_PID=""

cleanup() {
  if [[ -n "$MASTER_PID" ]] && kill -0 "$MASTER_PID" 2>/dev/null; then
    echo
    echo "[DEMO] Stopping master process..."
    kill "$MASTER_PID" 2>/dev/null || true
  fi
}

trap cleanup EXIT INT TERM

echo "[DEMO] Distributed Compute Engine local demo"
echo "[DEMO] Checking Python files..."
python3 -m py_compile master/master.py worker/worker1.py worker/worker2.py worker/worker3.py shell/dcesh.py protocol/*.py
echo "[DEMO] Syntax check passed"
echo

echo "[DEMO] Starting master on port 9000..."
python3 -m master.master &
MASTER_PID="$!"
sleep 2

echo
echo "[DEMO] Open three new terminals from this folder and run:"
echo "       python3 -m worker.worker1"
echo "       python3 -m worker.worker2"
echo "       python3 -m worker.worker3"
echo
echo "[DEMO] Wait until the master prints heartbeat messages."
read -r -p "[DEMO] Press Enter here to submit three sample jobs..."

submit_job() {
  local job_id="$1"
  local file_path="examples/distributed_sum.py"
  python3 - "$job_id" "$file_path" <<'PY'
import sys
from protocol.tcp_client import TCPClient

job_id = sys.argv[1]
file_path = sys.argv[2]

with open(file_path, "r", encoding="utf-8") as f:
    code = f.read()

client = TCPClient("127.0.0.1", 9000)
message = f"SUBMIT {job_id} {len(code)}\n{code}"
print(f"[DEMO] Submitting {job_id} from {file_path}")
print(client.send(message))
PY
}

submit_job "demo1"
submit_job "demo2"
submit_job "demo3"

echo
echo "[DEMO] Waiting for workers to finish..."
sleep 4

python3 - <<'PY'
from protocol.tcp_client import TCPClient

client = TCPClient("127.0.0.1", 9000)
for job_id in ["demo1", "demo2", "demo3"]:
    print(f"[DEMO] STATUS {job_id}")
    print(client.send(f"STATUS {job_id}"))
    print(f"[DEMO] LOGS {job_id}")
    print(client.send(f"LOGS {job_id}"))
PY

echo
echo "[DEMO] For the failure screenshot, stop one worker with Ctrl+C and wait about 20 seconds."
echo "[DEMO] Stop this script with Ctrl+C when finished."

while true; do
  sleep 5
done
