#!/usr/bin/env python3
"""Optional desktop Discord RPC bridge for an existing SwitchPresence-Rewritten server.
No Discord account token or Nintendo credentials are required.
"""
import argparse
import ipaddress
import socket
import struct
import time

PACKET = struct.Struct("<QQ612s")
MAGIC = 0xFFAADD23
PORT = 0xCAFE

def decode(packet):
    magic, title_id, raw = PACKET.unpack(packet)
    if magic != MAGIC:
        raise ValueError("Unrecognized presence packet")
    name = raw.split(b"\0", 1)[0].decode("utf-8", "replace")
    name = "".join(c for c in name if c.isprintable()).strip()[:128]
    return title_id, name

def receive(sock):
    data = bytearray()
    while len(data) < PACKET.size:
        part = sock.recv(PACKET.size - len(data))
        if not part:
            raise ConnectionError("Switch disconnected")
        data.extend(part)
    return decode(data)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("switch_ip")
    parser.add_argument("application_id", help="Your Discord application ID, not a secret")
    args = parser.parse_args()
    ip = ipaddress.IPv4Address(args.switch_ip)
    if not ip.is_private or ip.is_loopback or ip.is_link_local or ip.is_unspecified:
        parser.error("Use the Switch's private LAN IPv4 address")
    if not args.application_id.isdecimal() or not 16 <= len(args.application_id) <= 22:
        parser.error("Enter a valid Discord application ID")
    from pypresence import Presence
    rpc = Presence(args.application_id)
    rpc.connect()
    try:
        rpc.clear()
        print("Discord connected. Ctrl+C clears activity and exits.")
        last_update = 0.0
        while True:
            last = None
            started = 0
            published = False
            try:
                with socket.create_connection((str(ip), PORT), timeout=5) as sock:
                    sock.settimeout(5)
                    print("Switch connected.")
                    while True:
                        tid, name = receive(sock)
                        current = (tid, name) if tid and name else None
                        if current != last:
                            last = current
                            started = int(time.time())
                            published = False
                            if not current:
                                rpc.clear()
                                last_update = time.monotonic()
                        if current and not published and time.monotonic() - last_update >= 15:
                            rpc.update(details=("Playing " + name)[:128], state="Nintendo Switch · GlassNX", start=started)
                            last_update = time.monotonic()
                            published = True
            except (OSError, ValueError) as exc:
                rpc.clear()
                print("Switch unavailable; activity cleared.", type(exc).__name__)
                time.sleep(5)
    except KeyboardInterrupt:
        pass
    finally:
        try:
            rpc.clear()
        finally:
            rpc.close()

if __name__ == "__main__":
    main()
