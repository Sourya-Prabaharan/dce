from protocol.tcp_server import TCPServer
from protocol.tcp_client import TCPClient

# =====================================================
# WORKER REGISTRY (3 workers)
# =====================================================
workers = {
    "worker1": {"host": "127.0.0.1", "port": 9101, "status": "idle"},
    "worker2": {"host": "127.0.0.1", "port": 9102, "status": "idle"},
    "worker3": {"host": "127.0.0.1", "port": 9103, "status": "idle"},
}

# =====================================================
# JOB STATE
# jobID → {"code": str, "status": "QUEUED/RUNNING/DONE", "logs": ""}
# =====================================================
jobs = {}

# =====================================================
# ASSIGN JOB TO WORKER
# =====================================================
def assign_job(job_id, code):
    for wid, w in workers.items():
        if w["status"] == "idle":
            print(f"[MASTER] Assigning job {job_id} to {wid}")

            # connect to worker
            client = TCPClient(w["host"], w["port"])
            payload = f"RUN {job_id} {len(code)}\n{code}"
            client.send(payload)

            # mark worker busy and job running
            workers[wid]["status"] = "busy"
            jobs[job_id]["status"] = "RUNNING"
            return wid

    return None  # no free workers


# =====================================================
# MASTER MESSAGE HANDLER
# =====================================================
def handle_message(msg: str) -> str:
    print("[MASTER] Received:", msg)
    parts = msg.strip().split()

    # -------------------------------------------------
    # HANDLE SUBMIT
    # -------------------------------------------------
    if parts[0] == "SUBMIT":
        job_id = parts[1]
        code = msg.split("\n", 1)[1]   # everything after newline

        jobs[job_id] = {
            "code": code,
            "status": "QUEUED",
            "logs": "",
        }

        # Try assigning immediately
        assigned = assign_job(job_id, code)
        if assigned:
            return f"JOB {job_id} ASSIGNED TO {assigned}"
        else:
            return f"JOB {job_id} QUEUED"

    # -------------------------------------------------
    # HANDLE STATUS
    # -------------------------------------------------
    if parts[0] == "STATUS":
        job_id = parts[1]
        if job_id not in jobs:
            return "ERROR: job not found"
        return f"STATUS {job_id} {jobs[job_id]['status']}"

    # -------------------------------------------------
    # HANDLE LOGS
    # -------------------------------------------------
    if parts[0] == "LOGS":
        job_id = parts[1]
        if job_id not in jobs:
            return "ERROR: job not found"
        return f"LOGS {job_id}\n{jobs[job_id]['logs']}"

    # -------------------------------------------------
    # HANDLE DONE FROM WORKER
    # DONE <jobID> <output_size>\n<output>
    # -------------------------------------------------
    if parts[0] == "DONE":
        job_id = parts[1]
        print(f"[MASTER] Job {job_id} completed")

        jobs[job_id]["status"] = "DONE"

        # free the worker
        for wid in workers:
            if workers[wid]["status"] == "busy":
                workers[wid]["status"] = "idle"
        return f"ACK DONE {job_id}"

    # -------------------------------------------------
    # HEARTBEAT
    # -------------------------------------------------
    if parts[0] == "HEARTBEAT":
        wid = parts[1]
        # (Later we will track last heartbeat timestamp)
        return f"ACK HEARTBEAT {wid}"

    return "ERROR: unknown command"


# =====================================================
# START MASTER SERVER
# =====================================================
if __name__ == "__main__":
    print("[MASTER] Starting on port 9000")
    TCPServer("0.0.0.0", 9000, handle_message).start()

