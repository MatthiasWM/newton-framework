#!/usr/bin/env python3
"""
The wire to a Newton (plan step 3.1, Matt/NTKTransport.{h,cc}), tested
with `newtc -ntk-dump <target>`, which prints what arrives in hex:

  tcp:<port>        newtc listens; this test plays Einstein, whose serial
                    port connects as a TCP client: connect, send bytes,
                    disconnect, connect again (newtc keeps listening)
  serial:<device>   a pseudo-terminal plays the serial port of a Newton:
                    bytes arrive, the speed is set, closing the other end
                    is a disconnect
  bad targets       an error message and exit code 1

Usage: Test/ntk/test_transport.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import os
import queue
import socket
import subprocess
import sys
import termios
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Dump:
    """newtc -ntk-dump in the background; its stdout lines in a queue."""

    def __init__(self, newtc, target):
        self.proc = subprocess.Popen([newtc, "-ntk-dump", target], stdin=subprocess.DEVNULL,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.lines = queue.Queue()
        self.seen = []
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        for line in self.proc.stdout:
            self.lines.put(line.rstrip("\n"))

    def wait_for(self, start, timeout=5.0):
        """The next line starting with `start` (skipping others), or None."""
        end = time.time() + timeout
        while time.time() < end:
            try:
                line = self.lines.get(timeout=max(0.01, end - time.time()))
            except queue.Empty:
                break
            self.seen.append(line)
            if line.startswith(start):
                return line
        return None

    def received(self, count, timeout=5.0):
        """Collect `count` bytes from the "<-" lines (data may come in pieces)."""
        data = bytearray()
        end = time.time() + timeout
        while len(data) < count and time.time() < end:
            line = self.wait_for("ntk: <- ", max(0.01, end - time.time()))
            if line is None:
                break
            data += bytes.fromhex(line.split(":", 2)[2])
        return bytes(data)

    def stop(self):
        if self.proc.poll() is None:
            self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
        return self.proc.stderr.read()


def connect(port):
    for _ in range(100):            # until newtc listens
        try:
            return socket.create_connection(("127.0.0.1", port))
        except OSError:
            time.sleep(0.05)
    return None


def test_tcp(newtc, checks):
    port = free_port()
    dump = Dump(newtc, "tcp:%d" % port)
    checks.append(("tcp: says where it listens",
                   dump.wait_for("ntk: listening on") == "ntk: listening on 127.0.0.1:%d" % port))
    payload = bytes(range(256)) + b"\x16\x10\x02 an MNP frame start"
    for round_ in (1, 2):
        sock = connect(port)
        checks.append(("tcp: accepts Einstein (connection %d)" % round_, sock is not None))
        if not sock:
            break
        checks.append(("tcp: reports the connection (%d)" % round_,
                       (dump.wait_for("ntk: connected") or "").startswith("ntk: connected (127.0.0.1:")))
        sock.sendall(payload)
        checks.append(("tcp: every byte arrives (%d)" % round_, dump.received(len(payload)) == payload))
        sock.close()
        checks.append(("tcp: a disconnect, then listening again (%d)" % round_,
                       dump.wait_for("ntk: disconnected") == "ntk: disconnected, listening again"))
    err = dump.stop()
    checks.append(("tcp: no errors", err == ""))


def test_serial(newtc, checks):
    master, slave = os.openpty()
    device = os.ttyname(slave)
    dump = Dump(newtc, "serial:%s@57600" % device)
    checks.append(("serial: opens the port",
                   dump.wait_for("ntk: opened") == "ntk: opened %s at 57600 bps" % device))
    payload = b"\x16\x10\x02\x20\x01\x02" + bytes(range(100))
    os.write(master, payload)
    checks.append(("serial: the first bytes say connected",
                   dump.wait_for("ntk: connected") == "ntk: connected (%s at 57600 bps)" % device))
    checks.append(("serial: every byte arrives", dump.received(len(payload)) == payload))
    attrs = termios.tcgetattr(slave)
    checks.append(("serial: speed 57600, raw, 8 bits",
                   attrs[4] == termios.B57600 and attrs[5] == termios.B57600
                   and attrs[2] & termios.CSIZE == termios.CS8 and not attrs[3] & termios.ICANON))
    os.close(slave)
    os.close(master)                # the adapter is unplugged
    checks.append(("serial: unplugging is a disconnect", dump.wait_for("ntk: disconnected") is not None))
    try:
        dump.proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        pass
    checks.append(("serial: then newtc ends", dump.proc.returncode == 0))
    dump.stop()


def test_errors(newtc, checks):
    cases = [
        ("nonsense", "unknown target \"nonsense\""),
        ("tcp:99999", "bad port"),
        ("serial:", "no device"),
        ("serial:/dev/null@1234", "unsupported speed 1234"),
        ("serial:/nonexistent/tty", "can't open /nonexistent/tty"),
        ("serial:/dev/null", "is not a serial port"),
    ]
    for target, message in cases:
        r = subprocess.run([newtc, "-ntk-dump", target], capture_output=True, text=True, timeout=10)
        checks.append(("error for %s" % target, r.returncode == 1 and message in r.stderr))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []
    test_tcp(args.newtc, checks)
    test_serial(args.newtc, checks)
    test_errors(args.newtc, checks)
    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
