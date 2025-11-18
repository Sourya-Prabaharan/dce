import socket

class TCPClient:
    def __init__(self, host, port):
        self.host = host
        self.port = port

    def send(self, data: str) -> str:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((self.host, self.port))

        sock.sendall(data.encode())

        response = []
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response.append(chunk.decode())

        sock.close()
        return "".join(response)

