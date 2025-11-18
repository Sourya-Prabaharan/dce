import socket
import threading

class TCPServer:
    def __init__(self, host, port, handler):
        self.host = host
        self.port = port
        self.handler = handler  # function to process incoming messages

    def start(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.bind((self.host, self.port))
        server.listen(5)

        print(f"[TCPServer] Listening on {self.host}:{self.port}")

        while True:
            client_socket, addr = server.accept()
            thread = threading.Thread(
                target=self._handle_client,
                args=(client_socket, addr)
            )
            thread.start()

    def _handle_client(self, client_socket, addr):
        data = client_socket.recv(65536).decode()
        print(f"[TCPServer] Received from {addr}: {data}")

        response = self.handler(data)
        client_socket.sendall(response.encode())
        client_socket.close()

