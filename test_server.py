import socket
import struct
import time
import math

HOST = "0.0.0.0"
PORT = 8090

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind((HOST, PORT))
s.listen(5)
print(f"test server listening on {PORT} ...", flush=True)

t = 0
while True:
    conn, addr = s.accept()
    print(f"ESP32 connected from {addr}", flush=True)
    try:
        while True:
            left = 100.0 * math.sin(t)
            right = 100.0 * math.cos(t)
            data = struct.pack("<dd", left, right)   # два double, little-endian (как Power на ESP)
            conn.sendall(data)
            print(f"sent left={left:+.2f} right={right:+.2f}", flush=True)
            t += 0.5
            time.sleep(0.33)
    except (BrokenPipeError, ConnectionResetError):
        print("connection lost, waiting for new connection...", flush=True)
    finally:
        conn.close()
