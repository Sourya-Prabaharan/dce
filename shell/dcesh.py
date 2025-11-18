from protocol.tcp_client import TCPClient
from shell.parser import parse_command
import os

client = TCPClient("127.0.0.1", 9000)

print("Welcome to dcesh — Distributed Compute Engine Shell")
print("Type 'exit' to quit.")

job_counter = 0

while True:
    raw = input("dce> ")
    if raw.strip().lower() == "exit":
        break

    cmd = parse_command(raw)

    # SUBMIT <file.py>
    if cmd.type == "SUBMIT":
        file_path = cmd.args["file"]

        if not os.path.exists(file_path):
            print(f"File not found: {file_path}")
            continue

        with open(file_path, "r") as f:
            code = f.read()

        job_counter += 1
        job_id = f"job{job_counter}"

        message = f"SUBMIT {job_id} {len(code)}\n{code}"
        response = client.send(message)
        print(response)
        continue

    # STATUS <jobID>
    if cmd.type == "STATUS":
        message = f"STATUS {cmd.args['jobID']}"
        response = client.send(message)
        print(response)
        continue

    # LOGS <jobID>
    if cmd.type == "LOGS":
        message = f"LOGS {cmd.args['jobID']}"
        response = client.send(message)
        print(response)
        continue

    # fallback for unknown
    if cmd.type == "RAW":
        response = client.send(cmd.args["text"])
        print(response)

