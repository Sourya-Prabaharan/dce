from protocol.tcp_client import TCPClient

client = TCPClient("127.0.0.1", 9000)

print("Welcome to dcesh — Distributed Compute Engine Shell")
print("Type 'exit' to quit.")

while True:
    cmd = input("dce> ")

    if cmd.strip().lower() == "exit":
        break

    response = client.send(cmd)
    print("[MASTER RESPONSE]", response)

