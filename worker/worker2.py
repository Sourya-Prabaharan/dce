from protocol.tcp_server import TCPServer
from protocol.tcp_client import TCPClient
import threading
import time

WORKER_ID = "worker2"
MASTER_HOST = "127.0.0.1"
MASTER_PORT = 9000
WORKER_PORT = 9102

def heartbeat_loop():
    client = TCPClient(MASTER_HOST, MASTER_PORT)
    while True:
        try:
            client.send(f"HEARTBEAT {WORKER_ID}")
        except:
            pass
        time.sleep(5)

def handle_master_message(msg: str) -> str:
    print(f"[WORKER2] Received:", msg)

    parts = msg.split()

    if parts[0] == "RUN":
        job_id = parts[1]
        code = msg.split("\n", 1)[1] if "\n" in msg else ""

        print(f"[WORKER2] Running job {job_id} (fake run)")
        time.sleep(1)

        client = TCPClient(MASTER_HOST, MASTER_PORT)
        client.send(f"DONE {job_id} 0\n")

        return f"ACK RUN {job_id}"

    return "ERR"

if __name__ == "__main__":
    threading.Thread(target=heartbeat_loop, daemon=True).start()

    server = TCPServer("0.0.0.0", WORKER_PORT, handle_master_message)
    print(f"[WORKER2] Listening on port {WORKER_PORT}")
    server.start()

