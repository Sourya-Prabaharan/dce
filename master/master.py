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
# jobID → {"code": str, "status": "QUEUED/RUNNING/DONE", "logs": str}
# =====================================================
jobs = {}

# =====================================================
# Assign job to an idle worker
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
            jobs[job_id]["worker"] = wid
            return wid

    return None  # no free workers available


# =====================================================
# MASTER MESSAGE HANDLER
# =====================================================
def handle_message(msg: str) -> str:
    print("[MASTER] Received:", msg)
    parts = msg.strip().split()

    # -------------------------------------------------
    # SUBMIT job
    # -------------------------------------------------
    if parts[0] == "SUBMIT":
        job_id = parts[1]
        code = msg.split("\n", 1)[1]

        jobs[job_id] = {
            "code": code,
            "status": "QUEUED",
            "logs": "",
            "worker": None,
        }

        assigned = assign_job(job_id, code)
        if assigned:
            return f"JOB {job_id} ASSIGNED TO {assigned}"
        else:
            return f"JOB {job_id} QUEUED"

    # -------------------------------------------------
    # STATUS job
    # -------------------------------------------------
    if parts[0] == "STATUS":
        job_id = parts[1]
        if job_id not in jobs:
            return "ERROR: job not found"
        return f"STATUS {job_id} {jobs[job_id]['status']}"

    # -------------------------------------------------
    # LOGS job
    # -------------------------------------------------
    if parts[0] == "LOGS":
        job_id = parts[1]
        if job_id not in jobs:
            return "ERROR: job not found"
        logs = jobs[job_id]["logs"]
        return f"LOGS {job_id}\n{logs}"

    # -------------------------------------------------
    # DONE job (from worker)
    # DONE job1 <size>\n<output>
    # -------------------------------------------------
    if parts[0] == "DONE":
        job_id = parts[1]

        # Output is after the newline
        output = msg.split("\n", 1)[1] if "\n" in msg else ""

        jobs[job_id]["status"] = "DONE"
        jobs[job_id]["logs"] = output

        worker_used = jobs[job_id]["worker"]
        if worker_used:
            workers[worker_used]["status"] = "idle"

        print(f"[MASTER] Job {job_id} completed with output:")
        print(output)

        return f"ACK DONE {job_id}"

    # -------------------------------------------------
    # HEARTBEAT
    # -------------------------------------------------
    if parts[0] == "HEARTBEAT":
        wid = parts[1]
        return f"ACK HEARTBEAT {wid}"

    return "ERROR: unknown command"


# =====================================================
# START SERVER
# =====================================================
if __name__ == "__main__":
    print("[MASTER] Starting on port 9000")
    TCPServer("0.0.0.0", 9000, handle_message).start()

