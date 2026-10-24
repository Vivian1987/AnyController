#!/usr/bin/env python3
"""Send an ELF payload to a jailbroken PS5 via TCP."""

import socket
import sys
import os

DEFAULT_PORT = 9021

def send_payload(ip, payload_path, port=DEFAULT_PORT):
    if not os.path.isfile(payload_path):
        print(f"Error: {payload_path} not found")
        return False

    size = os.path.getsize(payload_path)
    print(f"Sending {payload_path} ({size} bytes) to {ip}:{port}...")

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(10)
            s.connect((ip, port))

            with open(payload_path, "rb") as f:
                data = f.read()
                s.sendall(data)

            print(f"Sent {size} bytes. Payload should be running on PS5.")
            return True

    except ConnectionRefusedError:
        print(f"Error: Connection refused. Is the PS5 jailbroken and listening on port {port}?")
    except socket.timeout:
        print(f"Error: Connection timed out. Check the PS5 IP address.")
    except Exception as e:
        print(f"Error: {e}")

    return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <PS5_IP> [payload.elf] [port]")
        print(f"  Default payload: anycontroller.elf")
        print(f"  Default port: {DEFAULT_PORT}")
        sys.exit(1)

    ip = sys.argv[1]
    payload = sys.argv[2] if len(sys.argv) > 2 else "anycontroller.elf"
    port = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_PORT

    success = send_payload(ip, payload, port)
    sys.exit(0 if success else 1)
