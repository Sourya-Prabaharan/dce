from protocol.tcp_server import TCPServer
from protocol.tcp_client import TCPClient
import threading
import time
import subprocess
import tempfile

WORKER_ID = "worker3"
MASTER_HOST = "127.0.0.1"
MASTER_PORT = 9000
WORKER_PORT = 9103

def run_job_in_docker(job_id, code):
    with tempfile.NamedTemporaryFile(delete=False, suffix=".py") as tmp:
        tmp.write(code.encode())
        tmp_path = tmp.name

    print(f"[WORKER worker3] Prepared task {job_id}")

    cmd = [
        "docker", "run", "--rm",
        "-v", f"{tmp_path}:/app/job.py",
        "python:3.10",
        "python", "/app/job.py"
    ]

    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        output_text = output.decode()
    except subprocess.CalledProcessError as e:
        output_text = e.output.decode()

    return output_text

def handle_master_message(msg: str) -> str:
    parts = msg.split()

    if parts[0] == "RUN":
        job_id = parts[1]
        code = msg.split("\n", 1)[1]

        print(f"[WORKER worker3] Received task {job_id}")
        print(f"[WORKER worker3] Running task {job_id} in Docker")
        output = run_job_in_docker(job_id, code)
        print(f"[WORKER worker3] Completed task {job_id}")

        client = TCPClient(MASTER_HOST, MASTER_PORT)
        payload = f"DONE {job_id} {len(output)}\n{output}"
        client.send(payload)

        return f"ACK RUN {job_id}"

    return "ERR"

def heartbeat_loop():
    client = TCPClient(MASTER_HOST, MASTER_PORT)
    while True:
        try:
            client.send(f"HEARTBEAT {WORKER_ID}")
        except:
            pass
        time.sleep(5)

if __name__ == "__main__":
    threading.Thread(target=heartbeat_loop, daemon=True).start()

    server = TCPServer("0.0.0.0", WORKER_PORT, handle_master_message)
    print(f"[WORKER worker3] Listening on port {WORKER_PORT}")
    server.start()
