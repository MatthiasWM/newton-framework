#!/usr/bin/env python3
"""
The Toolkit packets in newtc (plan step 3.3, Matt/NTKInspector.{h,cc}),
tested with the terminal Inspector `newtc -ntk tcp:<port>` against a fake
Newton (MNP from test_mnp.py) that plays the device side with replies as
captured from Einstein (Matt/Toolkit Protocol.md, section 6):

  - lines typed before the Newton is there wait for it
  - 'cnnt' is answered with 'okln' (length 0); the 'dante' fobj is shown
  - a line goes as 'lscb' with a code block (NSOF) compiled by newtc; the
    REP's 'rslt' and 'text' come back (CR -> LF)
  - a line starting with "=" goes as 'code' (u32 0 + NSOF); the reply's
    object is printed
  - 'eref', 'eerr', 'estr' are printed like newtc's REPL prints exceptions
  - 'eext', 'bext', 'fstk'
  - a compile error stays on the desktop
  - the end of stdin: a last 'code' (the sync), then 'term'

Usage: Test/ntk/test_inspector.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
from test_mnp import FakeNewton, Lines, NEWTON_LR, DESKTOP_LR, free_port, LT, LA  # noqa: E402


class NewtonSide(FakeNewton):
    """MNP as the Newton plus the Toolkit packets."""

    def __init__(self, port):
        super().__init__(port)
        self.seq = 0
        self.received = bytearray()

    def link(self):
        self.send(NEWTON_LR, bytewise=False)
        ok = self.next_frame() == DESKTOP_LR
        self.send(bytes([3, LA, 0, 8]))
        return ok

    def send_data(self, data):
        for i in range(0, len(data), 256):
            self.seq = (self.seq + 1) & 0xFF
            self.send(bytes([2, LT, self.seq]) + data[i:i + 256], bytewise=False)
            while True:                   # its LA (keep-alives may come too)
                f = self.next_frame(3.0)
                if f is None or (f[1] == LA and f[2] == self.seq):
                    break
                self._take(f)

    def packet(self, cmd, data=b"", length=None):
        length = len(data) if length is None else length
        self.send_data(b"newtntp " + cmd + struct.pack(">I", length) + data)

    def _take(self, f):
        if f[1] == LT:
            self.received += f[3:]
            self.send(bytes([3, LA, f[2], 1]), bytewise=False)

    def next_packet(self, timeout=5.0):
        """The next Toolkit packet from newtc: (cmd, length, data)."""
        end = time.time() + timeout
        while time.time() < end:
            if len(self.received) >= 16:
                cmd = bytes(self.received[8:12])
                length = struct.unpack(">I", self.received[12:16])[0]
                size = length
                if len(self.received) >= 16 + size:
                    data = bytes(self.received[16:16 + size])
                    del self.received[:16 + size]
                    return cmd, length, data
            f = self.next_frame(max(0.01, end - time.time()))
            if f is not None:
                self._take(f)
        return None


def nsof(newtc, expr, tmp):
    path = os.path.join(tmp, "x.nsof")
    subprocess.run([newtc, "-s", expr, "-onsof", path], check=True, capture_output=True)
    return Path(path).read_bytes()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    tmp = tempfile.mkdtemp()
    err_frame = nsof(args.newtc, "{errorCode: -48808, symbol: 'NoSuchFunctionProbe}", tmp)
    stack = nsof(args.newtc, "[{class: 'StackFrameInfoFrame, CodeBlock: \"functions.BreakLoop\", "
                             "programCounter: -1, receiver: nil, contextFrame: nil}]", tmp)
    dante = nsof(args.newtc, "{interpretation: 'dante, data: {}}", tmp)

    port = free_port()
    proc = subprocess.Popen([args.newtc, "-ntk", "tcp:%d" % port], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    out = Lines(proc.stdout)
    errs = Lines(proc.stderr)
    check("says where it waits", out.wait_for("ntk: waiting for a Newton on") ==
          "ntk: waiting for a Newton on 127.0.0.1:%d" % port)
    # typed before the Newton is there: waits
    proc.stdin.write('Print("hi")\n= 2+3\nNoSuchFunctionProbe()\n')
    proc.stdin.flush()

    newton = NewtonSide(port)
    check("MNP link", newton.link())
    newton.packet(b"cnnt")
    check("answers cnnt with okln, length 0", newton.next_packet() == (b"okln", 0, b""))
    check("says it is connected", (out.wait_for("ntk: connected") or "").startswith("ntk: connected (127.0.0.1:"))
    newton.packet(b"fobj", dante)
    line = out.wait_for("ntk: fobj") or ""
    check("shows the 'dante' fobj", line.startswith("ntk: fobj {interpretation: 'dante,")
          and out.wait_containing("data: {}}") is not None)

    # line 1: lscb
    p = newton.next_packet()
    check("a line goes as lscb with NSOF", p is not None and p[0] == b"lscb" and p[1] == len(p[2])
          and p[2][:1] == b"\x02" and b"Print" in p[2])
    newton.packet(b"rslt", struct.pack(">i", 0))
    newton.packet(b"text", b'"hi"\r#2        NIL\r')
    check("text arrives, CR as LF", out.wait_for('"hi"') == '"hi"' and out.wait_for("#2") == "#2        NIL")

    # line 2: code
    p = newton.next_packet()
    check("'=' goes as code: u32 0, then NSOF", p is not None and p[0] == b"code"
          and p[2][:5] == b"\x00\x00\x00\x00\x02")
    newton.packet(b"rslt", struct.pack(">i", 0))
    newton.packet(b"code", struct.pack(">I", 3) + b"\x02\x00\x14", length=p[1])   # 5, length echoed
    check("the code result is printed", out.wait_for("5") == "5")

    # line 3: lscb, then an eref
    p = newton.next_packet()
    check("line 3 as lscb", p is not None and p[0] == b"lscb")
    name = b"evt.ex.fr.intrp;type.ref.frame\x00"
    newton.packet(b"rslt", struct.pack(">i", 0))
    newton.packet(b"eref", struct.pack(">I", len(name)) + name + struct.pack(">I", len(err_frame)) + err_frame,
                  length=len(name) + len(err_frame))
    check("eref printed like the REPL",
          out.wait_containing("!!! Exception: Undefined global function") is not None)

    # eerr and estr (length + 4, length + 8)
    name = b"evt.ex.fr.intrp\x00"
    newton.packet(b"eerr", struct.pack(">I", len(name)) + name + struct.pack(">i", -48800),
                  length=len(name) + 4)
    check("eerr printed with its text", out.wait_containing("!!! Exception: Not in a break loop") is not None)
    name, msg = b"evt.ex.msg\x00", b"a message\x00"
    newton.packet(b"estr", struct.pack(">I", len(name)) + name + struct.pack(">I", len(msg)) + msg,
                  length=len(name) + len(msg))
    check("estr printed with its message", out.wait_containing("!!! Exception: a message") is not None)

    # break loop, stack
    newton.packet(b"eext")
    check("eext: break loop entered", out.wait_for("ntk: break loop") == "ntk: break loop entered")
    newton.packet(b"fstk", stack)
    check("fstk shown", "functions.BreakLoop" in (out.wait_for("ntk: fstk") or "")
          or "functions.BreakLoop" in " ".join(out.all[-8:]))
    newton.packet(b"bext")
    check("bext: break loop left", out.wait_for("ntk: break loop") == "ntk: break loop left")

    # a compile error stays here
    proc.stdin.write("1 +* 2\n")
    proc.stdin.flush()
    check("a compile error is shown", out.wait_containing("syntax error", 3.0) is not None)
    check("... and nothing is sent", newton.next_packet(1.0) is None)

    # the end of stdin while a code call is still unanswered: its result is
    # printed, the sync's isn't (Einstein, 2026-10-05: the sync hid "42")
    proc.stdin.write("= 6*7\n")
    proc.stdin.close()
    user = newton.next_packet()
    p = newton.next_packet()
    check("end of input: a last code call (the sync)", user is not None and user[0] == b"code"
          and p is not None and p[0] == b"code")
    if p:
        newton.packet(b"rslt", struct.pack(">i", 0))
        newton.packet(b"code", struct.pack(">I", 3) + b"\x02\x00\xa8", length=user[1])   # 42
        newton.packet(b"rslt", struct.pack(">i", 0))
        newton.packet(b"code", struct.pack(">I", 2) + b"\x02\x0a", length=p[1])      # nil
    check("a reply still pending at the end is printed", out.wait_for("42") == "42")
    check("then term", newton.next_packet() == (b"term", 0, b""))
    newton.send(bytes([7, 2, 1, 1, 0, 2, 1, 0]))      # LD, as Einstein answers term
    check("and ends", out.wait_for("ntk: done", 5.0) == "ntk: done")
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
    check("exit code 0", proc.returncode == 0)
    check("the sync's nil isn't printed", "nil" not in out.all)
    time.sleep(0.2)
    check("no sanitizer reports", not any("Sanitizer" in l or "runtime error" in l for l in errs.raw))
    newton.close()

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n--- stdout ---\n" + "\n".join(out.raw) + "\n--- stderr ---\n" + "\n".join(errs.raw))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
