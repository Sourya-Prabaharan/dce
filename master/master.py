from protocol.tcp_server import TCPServer

def handle_message(msg: str) -> str:
    print("[MASTER] Handling:", msg)
    return "ACK_FROM_MASTER"

if __name__ == "__main__":
    server = TCPServer("0.0.0.0", 9000, handle_message)
    server.start()

