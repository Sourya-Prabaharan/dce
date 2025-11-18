from protocol.tcp_server import TCPServer
from protocol.tcp_client import TCPClient
import time
import threading

# =====================================================
# WORKER REGISTRY
# =====================================================
workers = {
    "worker1": {"host": "127.0.0.1", "port": 9101, "status": "idle", "last_heartbeat": 0},
    "worker2": {"host": "127.0.0.1", "port": 9102, "status": "idle", "last_heartbeat": 0},
    "worker3": {"host": "127.0.0.1", "port": 9103, "status": "idle", "last_heartbeat": 0},
}

# =====================================================
# JOB STATE + FIFO QUEUE
# =====================================================
jobs = {}
job_queue = []   # FIFO order of waiting jobs


# =====================================================
# Assign job to a healthy worker
# =====================================================
def assign_job(job_id, code):
    for wid, w in workers.items():

        # worker must be alive and idle
        if w["status"] == "idle" and w["last_heartbeat"] != 0:

            print(f"[MASTER] Assigning job {job_id} to {wid}")

            try:
                client = TCPClient(w["host"], w["port"])
                payload = f"RUN {job_id} {len(code)}\n{code}"
                client.send(payload)
            except:
                print(f"[MASTER] Worker {wid} unreachable — marking dead")
                w["status"] = "dead"
                continue

            workers[wid]["status"] = "busy"
            jobs[job_id]["status"] = "RUNNING"
            jobs[job_id]["worker"] = wid
            return wid

    return None


# =====================================================
# Try assigning queued jobs repeatedly
# =====================================================
def scheduler_loop():
    while True:
        time.sleep(1)

        if not job_queue:
            continue

        job_id = job_queue[0]
        code = jobs[job_id]["code"]

        assigned = assign_job(job_id, code)
        if assigned:
            print(f"[MASTER] Scheduled queued job {job_id}")
            job_queue.pop(0)  # remove from queue


# =====================================================
# MESSAGE HANDLER
# =====================================================
def handle_message(msg: str) -> str:
    print("[MASTER] Received:", msg)
    parts = msg.strip().split()
    cmd = parts[0]

    # -------------------------------
    # SUBMIT job
    # -------------------------------
    if cmd == "SUBMIT":
        job_id = parts[1]
        code = msg.split("\n", 1)[1]

        jobs[job_id] = {
            "code": code,
            "status": "QUEUED",
            "logs": "",
            "worker": None,
        }

        # Try direct assignment
        assigned = assign_job(job_id, code)
        if assigned:
            return f"JOB {job_id} ASSIGNED TO {assigned}"
        else:
            job_queue.append(job_id)
            return f"JOB {job_id} QUEUED"

    # -------------------------------
    # STATUS
    # -------------------------------
    if cmd == "STATUS":
        job_id = parts[1]
        if job_id not in jobs:
            return "ERROR: job not found"
        return f"STATUS {job_id} {jobs[job_id]['status']}"

    # -------------------------------
    # LOGS
    # -------------------------------
    if cmd == "LOGS":
        job_id = parts[1]
        if job_id not in jobs:
            return "ERROR: job not found"
        return f"LOGS {job_id}\n{jobs[job_id]['logs']}"

    # -------------------------------
    # DONE
    # -------------------------------
    if cmd == "DONE":
        job_id = parts[1]
        output = msg.split("\n", 1)[1] if "\n" in msg else ""

        jobs[job_id]["status"] = "DONE"
        jobs[job_id]["logs"] = output

        wid = jobs[job_id]["worker"]
        if wid:
            workers[wid]["status"] = "idle"

        print(f"[MASTER] Job {job_id} completed with output:")
        print(output)

        return f"ACK DONE {job_id}"

    # -------------------------------
    # HEARTBEAT
    # -------------------------------
    if cmd == "HEARTBEAT":
        wid = parts[1]
        if wid in workers:
            workers[wid]["last_heartbeat"] = time.time()
            if workers[wid]["status"] == "dead":
                workers[wid]["status"] = "idle"
        return f"ACK HEARTBEAT {wid}"

    return "ERROR: unknown command"


# =====================================================
# DEAD WORKER MONITOR
# =====================================================
HEARTBEAT_TIMEOUT = 15

def worker_monitor():
    while True:
        time.sleep(5)
        now = time.time()

        for wid, w in workers.items():
            # Worker never heartbeat yet
            if w["last_heartbeat"] == 0:
                continue

            # Timeout → worker dead
            if now - w["last_heartbeat"] > HEARTBEAT_TIMEOUT:
                if w["status"] != "dead":
                    print(f"[MASTER] Worker {wid} appears DEAD")
                    w["status"] = "dead"

                    # Requeue running job
                    for job_id, job in jobs.items():
                        if job["worker"] == wid and job["status"] == "RUNNING":
                            print(f"[MASTER] Requeueing job {job_id}")
                            job["status"] = "QUEUED"
                            job["worker"] = None
                            job_queue.append(job_id)


# =====================================================
# START SERVER + THREADS
# =====================================================
if __name__ == "__main__":
    print("[MASTER] Starting on port 9000")

    # Start scheduler thread
    threading.Thread(target=scheduler_loop, daemon=True).start()

    # Start worker death monitor
    threading.Thread(target=worker_monitor, daemon=True).start()

    # Start TCP Server
    TCPServer("0.0.0.0", 9000, handle_message).start()

