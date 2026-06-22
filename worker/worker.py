from protocol.tcp_server import TCPServer
from protocol.tcp_client import TCPClient
import threading
import time

WORKER_ID = "worker1"
MASTER_HOST = "127.0.0.1"
MASTER_PORT = 9000
WORKER_PORT = 9101   # you can increment this for worker2, worker3

# Send heartbeat to master
def heartbeat_loop():
    client = TCPClient(MASTER_HOST, MASTER_PORT)
    while True:
        try:
            client.send(f"HEARTBEAT {WORKER_ID}")
        except:
            pass
        time.sleep(5)

def handle_master_message(msg: str) -> str:
    parts = msg.split()

    if parts[0] == "RUN":
        job_id = parts[1]
        code = msg.split("\n", 1)[1]

        # TEMP: Just fake run (Phase 4 will execute Docker)
        print(f"[WORKER {WORKER_ID}] Received task {job_id}")
        print(f"[WORKER {WORKER_ID}] Running task {job_id}")
        time.sleep(1)

        # Send completion back to master
        client = TCPClient(MASTER_HOST, MASTER_PORT)
        client.send(f"DONE {job_id} 0\n")

        return f"ACK RUN {job_id}"

    return "ERR"

if __name__ == "__main__":
    # Start heartbeat thread
    threading.Thread(target=heartbeat_loop, daemon=True).start()

    # Start worker server
    server = TCPServer("0.0.0.0", WORKER_PORT, handle_master_message)
    print(f"[WORKER {WORKER_ID}] Listening on port {WORKER_PORT}")
    server.start()
