#!/usr/bin/env python3
"""
MNP in newtc (plan step 3.2, Matt/MNP.{h,cc}), tested with
`newtc -ntk-mnp tcp:<port>` against a fake Newton that connects like
Einstein (bytes one at a time) and checks every frame newtc sends:

  - the Newton's LR (Einstein's, as captured), sent twice: newtc answers
    each with the desktop LR; the Newton's LA brings the link up
  - LTs from the Newton: acknowledged, delivered once (a resent LT is
    acknowledged again but not delivered), DLE bytes in the data
  - a frame with a bad CRC: newtc acknowledges what it has
  - data from newtc (stdin): split into LTs of 256 bytes, each waiting for
    its LA; an LT without LA is sent again after about a second
  - a keep-alive LA after 3 s of quiet
  - the Newton's LD: link down; a new LR on the same connection: up again
  - the connection closed, a new one: link up again
  - the end of stdin: newtc sends an LD and stops

Usage: Test/ntk/test_mnp.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import queue
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
from ntk_probe import crc16  # noqa: E402

# Einstein's LR (capture0 and -ntk-dump, 2026-10-05)
NEWTON_LR = bytes.fromhex("20 01 02 01 06 01 00 00 00 00 ff 02 01 02 03 01 08 04 02 40 00"
                          "08 01 03 09 01 01 0e 04 03 04 00 fa")
DESKTOP_LR = bytes.fromhex("17 01 02 01 06 01 00 00 00 00 ff 02 01 02 03 01 01 04 02 40 00"
                           "08 01 03")
LT, LA, LD, LR = 4, 5, 2, 1


def encode(frame):
    out = bytearray(b"\x16\x10\x02")
    for b in frame:
        out.append(b)
        if b == 0x10:
            out.append(0x10)
    out += b"\x10\x03"
    crc = crc16(frame + b"\x03")
    out += bytes([crc & 0xFF, crc >> 8])
    return bytes(out)


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Lines:
    """A stream's lines in a queue, read in a thread."""

    def __init__(self, stream):
        self.q = queue.Queue()
        self.all = []
        self.raw = []                   # every line, also those nobody waited for
        threading.Thread(target=self._read, args=(stream,), daemon=True).start()

    def _read(self, stream):
        for line in stream:
            self.raw.append(line.rstrip("\n"))
            self.q.put(line.rstrip("\n"))

    def wait_for(self, start, timeout=5.0):
        end = time.time() + timeout
        while time.time() < end:
            try:
                line = self.q.get(timeout=max(0.01, end - time.time()))
            except queue.Empty:
                break
            self.all.append(line)
            if line.startswith(start):
                return line
        return None

    def wait_containing(self, text, timeout=5.0):
        """The next line containing `text` (skipping others), or None."""
        end = time.time() + timeout
        while time.time() < end:
            try:
                line = self.q.get(timeout=max(0.01, end - time.time()))
            except queue.Empty:
                break
            self.all.append(line)
            if text in line:
                return line
        return None

    def none_of(self, start, seconds):
        """No line starting with `start` within `seconds`."""
        return self.wait_for(start, seconds) is None


class FakeNewton:
    def __init__(self, port):
        self.sock = None
        for _ in range(100):
            try:
                self.sock = socket.create_connection(("127.0.0.1", port))
                break
            except OSError:
                time.sleep(0.05)
        self.frames = queue.Queue()
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        state, frame, crc = 0, bytearray(), bytearray()
        while True:
            try:
                data = self.sock.recv(4096)
            except OSError:
                break
            if not data:
                break
            for b in data:
                if state == 0:
                    state = 1 if b == 0x16 else 0
                elif state == 1:
                    state = 2 if b == 0x10 else 0
                elif state == 2:
                    state, frame = (3, bytearray()) if b == 0x02 else (0, frame)
                elif state == 3:
                    if b == 0x10:
                        state = 4
                    else:
                        frame.append(b)
                elif state == 4:
                    if b == 0x10:
                        frame.append(b)
                        state = 3
                    elif b == 0x03:
                        state, crc = 5, bytearray()
                    else:
                        state = 0
                elif state == 5:
                    crc.append(b)
                    if len(crc) == 2:
                        state = 0
                        ok = (crc[0] | crc[1] << 8) == crc16(bytes(frame) + b"\x03")
                        self.frames.put(bytes(frame) if ok else b"BAD CRC")

    def send(self, frame, bytewise=True):
        data = encode(frame)
        if bytewise:                    # like Einstein's serial port emulation
            for b in data:
                self.sock.sendall(bytes([b]))
        else:
            self.sock.sendall(data)

    def send_raw(self, data):
        self.sock.sendall(data)

    def next_frame(self, timeout=3.0, skip_la=False):
        end = time.time() + timeout
        while time.time() < end:
            try:
                f = self.frames.get(timeout=max(0.01, end - time.time()))
            except queue.Empty:
                return None
            if skip_la and len(f) > 1 and f[1] == LA:
                continue                # keep-alives may come any time
            return f
        return None

    def lt(self, seq, data):
        self.send(bytes([2, LT, seq]) + data)

    def la(self, seq):
        self.send(bytes([3, LA, seq, 1]))

    def close(self):
        self.sock.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    port = free_port()
    proc = subprocess.Popen([args.newtc, "-ntk-mnp", "tcp:%d" % port], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    out = Lines(proc.stdout)
    err = Lines(proc.stderr)
    check("says where it waits", out.wait_for("mnp: waiting on") == "mnp: waiting on 127.0.0.1:%d" % port)

    newton = FakeNewton(port)
    check("reports the connection", (out.wait_for("mnp: connected") or "").startswith("mnp: connected (127.0.0.1:"))

    # the handshake; the Newton resends its LR once
    newton.send(bytes([0x20]) + NEWTON_LR[1:])
    check("answers the LR with the desktop LR", newton.next_frame() == DESKTOP_LR)
    newton.send(NEWTON_LR)
    check("answers a resent LR again", newton.next_frame() == DESKTOP_LR)
    newton.send(bytes([3, LA, 0, 8]))
    check("the Newton's LA brings the link up", out.wait_for("mnp: link up") == "mnp: link up")

    # LTs from the Newton
    cnnt = b"newtntp cnnt\x00\x00\x00\x00"
    newton.lt(1, cnnt)
    check("acknowledges LT 1", newton.next_frame() == bytes([3, LA, 1, 1]))
    check("delivers LT 1", out.wait_for("mnp: <- ") == "mnp: <- 16 bytes: " + cnnt.hex(" "))
    newton.lt(1, cnnt)
    check("acknowledges a resent LT again", newton.next_frame() == bytes([3, LA, 1, 1]))
    check("... but doesn't deliver it twice", out.none_of("mnp: <- ", 0.5))
    dle = bytes([0x10, 0x10, 0x16, 0x10, 0x02, 0x10, 0x03, 0x33])
    newton.lt(2, dle)
    check("acknowledges LT 2", newton.next_frame() == bytes([3, LA, 2, 1]))
    check("DLE bytes arrive as they are", out.wait_for("mnp: <- ") == "mnp: <- 8 bytes: " + dle.hex(" "))
    bad = bytearray(encode(bytes([2, LT, 3]) + b"broken"))
    bad[-1] ^= 0xFF
    newton.send_raw(bytes(bad))
    check("a bad CRC: acknowledges what it has (LT 2)", newton.next_frame() == bytes([3, LA, 2, 1]))
    check("... and delivers nothing", out.none_of("mnp: <- ", 0.5))

    # data from newtc: 300 bytes = LT of 256 + LT of 44
    payload = bytes(range(256)) + bytes(range(44))
    proc.stdin.write(" ".join("%02x" % b for b in payload) + "\n")
    proc.stdin.flush()
    f1 = newton.next_frame(skip_la=True)
    check("sends LT 1 with 256 bytes", f1 == bytes([2, LT, 1]) + payload[:256])
    check("... and waits for its LA", newton.next_frame(0.5, skip_la=True) is None)
    newton.la(1)
    f2 = newton.next_frame(skip_la=True)
    check("then LT 2 with the rest", f2 == bytes([2, LT, 2]) + payload[256:])
    newton.la(2)

    # no LA: sent again after about a second
    proc.stdin.write("6e 65 77 74\n")
    proc.stdin.flush()
    first = newton.next_frame(skip_la=True)
    t0 = time.time()
    again = newton.next_frame(3.0, skip_la=True)
    waited = time.time() - t0
    check("sends LT 3", first == bytes([2, LT, 3]) + b"newt")
    check("sends it again after about 1 s without LA", again == first and 0.7 < waited < 2.0)
    newton.la(3)

    # keep-alive after 3 s of quiet
    while newton.next_frame(0.2) is not None:
        pass
    t0 = time.time()
    ka = newton.next_frame(5.0)
    check("keep-alive LA after about 3 s", ka == bytes([3, LA, 2, 1]) and 2.5 < time.time() - t0 < 4.0)

    # the Newton's LD, then a new link on the same connection
    newton.send(bytes([7, LD, 1, 1, 0, 2, 1, 0]))     # as Einstein sends it on 'term'
    check("the Newton's LD: link down",
          out.wait_for("mnp: link down") == "mnp: link down (the Newton disconnected)")
    newton.send(NEWTON_LR)
    check("a new LR on the same connection: answered", newton.next_frame(skip_la=True) == DESKTOP_LR)
    newton.send(bytes([3, LA, 0, 8]))
    check("... link up again", out.wait_for("mnp: link up") == "mnp: link up")
    newton.lt(1, b"seq starts at 1 again")
    check("... numbering starts again", newton.next_frame() == bytes([3, LA, 1, 1]))

    # the connection closed, a new one
    newton.close()
    check("a closed connection: link down",
          out.wait_for("mnp: link down") == "mnp: link down (the connection is gone)")
    newton = FakeNewton(port)
    check("a new connection is accepted", out.wait_for("mnp: connected") is not None)
    newton.send(NEWTON_LR)
    check("... and linked", newton.next_frame() == DESKTOP_LR)
    newton.send(bytes([3, LA, 0, 8]))
    check("... link up", out.wait_for("mnp: link up") == "mnp: link up")

    # end of stdin: LD, stop
    proc.stdin.close()
    ld = newton.next_frame(skip_la=True)
    check("end of stdin: sends an LD", ld == bytes([4, LD, 1, 1, 0xFF]))
    check("... and stops", out.wait_for("mnp: stopped") == "mnp: stopped")
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
    check("exit code 0", proc.returncode == 0)
    check("traces frames on stderr", err.wait_for("mnp: <- LR", 1.0) is not None)
    time.sleep(0.2)
    check("no sanitizer reports", not any("Sanitizer" in l or "runtime error" in l for l in err.raw))
    newton.close()

    # stdin from a FIFO (as a script drives it): closing the writer ends
    # newtc. (poll() on macOS missed this; select() doesn't.)
    import os
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        fifo = os.path.join(tmp, "in.fifo")
        os.mkfifo(fifo)
        p2 = subprocess.Popen("exec %s -ntk-mnp tcp:%d < %s" % (args.newtc, free_port(), fifo),
                              shell=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        writer = open(fifo, "w")
        out2 = Lines(p2.stdout)
        out2.wait_for("mnp: waiting on")
        writer.close()
        check("stdin from a FIFO: closing it ends newtc", out2.wait_for("mnp: stopped", 3.0) == "mnp: stopped")
        if p2.poll() is None:
            p2.kill()

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n--- stdout ---\n" + "\n".join(out.all))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
