from protocol.tcp_server import TCPServer
from protocol.tcp_client import TCPClient
import threading
import time
import subprocess
import tempfile

WORKER_ID = "worker1"
MASTER_HOST = "127.0.0.1"
MASTER_PORT = 9000
WORKER_PORT = 9101

# --------------------------------------------------
# Execute job inside Docker container
# --------------------------------------------------
def run_job_in_docker(job_id, code):
    # Save code to temporary .py file
    with tempfile.NamedTemporaryFile(delete=False, suffix=".py") as tmp:
        tmp.write(code.encode())
        tmp_path = tmp.name

    print(f"[WORKER1] Saved code to {tmp_path}")

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

# --------------------------------------------------
# Handle master → worker messages
# --------------------------------------------------
def handle_master_message(msg: str) -> str:
    print(f"[WORKER1] Received:", msg)

    parts = msg.split()

    if parts[0] == "RUN":
        job_id = parts[1]
        code = msg.split("\n", 1)[1]  # code after newline

        print(f"[WORKER1] Running job {job_id} in Docker")
        output = run_job_in_docker(job_id, code)

        # Send DONE + output back to master
        client = TCPClient(MASTER_HOST, MASTER_PORT)
        payload = f"DONE {job_id} {len(output)}\n{output}"
        client.send(payload)

        return f"ACK RUN {job_id}"

    return "ERR"

# --------------------------------------------------
# Heartbeat thread
# --------------------------------------------------
def heartbeat_loop():
    client = TCPClient(MASTER_HOST, MASTER_PORT)
    while True:
        try:
            client.send(f"HEARTBEAT {WORKER_ID}")
        except:
            pass
        time.sleep(5)

# --------------------------------------------------
# Start worker server
# --------------------------------------------------
if __name__ == "__main__":
    threading.Thread(target=heartbeat_loop, daemon=True).start()

    server = TCPServer("0.0.0.0", WORKER_PORT, handle_master_message)
    print(f"[WORKER1] Listening on port {WORKER_PORT}")
    server.start()

