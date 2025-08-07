# client.py
# https://github.com/WhiteMonsterZeroUltraEnergy
# MIT License

import socket
import sys

try:
    HOST = sys.argv[1]
    PORT = int(sys.argv[2])
except IndexError:
    print("Usage: python main.py HOST PORT")
    exit(1)

with socket.create_connection((HOST, PORT)) as sock:
    print(f"Connected to {HOST}:{PORT}\nType \"exit\" to end the connection.", file=sys.stderr)
    while True:
        msg = input("> ")
        if msg.lower() == "exit":
            break
        sock.sendall(msg.encode())
        response = sock.recv(1024)
        print(response.decode(), file=sys.stdout)

print("[*] Connection closed.", file=sys.stderr)
