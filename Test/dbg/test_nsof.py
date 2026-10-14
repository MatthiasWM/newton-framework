#!/usr/bin/env python3
"""
NSOF written by newtc (-onsof) reads back (-nsof) as it was:
  - B49: a magic pointer stays a magic pointer (newtc wrote the ROM object
    it stands for, cut short: unreadable)
  - B48: a frame whose _proto isn't its last slot keeps all its other slots
    (the writer counted them without _proto and left out the last one)
  - -onsof writes _proto slots, -onsof_no_proto leaves them all out

Usage: Test/dbg/test_nsof.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

CASES = [
    ("@180", "@180"),
    ("{c: {k: 42, p: @180}}", "{c: {k: 42, p: @180}}"),
    ("[protoFloatNGo, protoTextButton]", "[@180, @226]"),
    # -onsof keeps _proto slots: a ROM proto as its magic pointer, a frame
    # with its slots
    ("{_proto: @180, a: 1, b: 2}", "{_proto: @180, a: 1, b: 2}"),
    ("{a: 1, _proto: {x: 1}, b: 2}", "{a: 1, _proto: {x: 1}, b: 2}"),
]

# -onsof_no_proto leaves out every _proto slot (as the ROM's data): the
# frame keeps its other slots (B48: the last one was lost)
NO_PROTO_CASES = [
    ("{_proto: @180, a: 1, b: 2}", "{a: 1, b: 2}"),
    ("{a: 1, _proto: {x: 1}, b: 2}", "{a: 1, b: 2}"),
    ("{a: {_proto: @226, text: \"t\"}}", "{a: {text: \"t\"}}"),
]


def run(newtc, *args):
    return subprocess.run([newtc, *args], capture_output=True, text=True,
                          stdin=subprocess.DEVNULL, timeout=30)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    failed = 0
    with tempfile.TemporaryDirectory() as tmp:
        nsof = str(Path(tmp) / "x.nsof")
        for option, expr, expected in [("-onsof", *c) for c in CASES] + [("-onsof_no_proto", *c) for c in NO_PROTO_CASES]:
            write = run(args.newtc, "-s", expr, option, nsof)
            read = run(args.newtc, "-nsof", nsof, "-s", "Print(ref0)")
            got = " ".join(read.stdout.split())
            ok = write.returncode == 0 and read.returncode == 0 and got == expected
            print(("ok      " if ok else "FAIL    ") + "%s %s -> %s" % (option, expr, got or read.stdout + read.stderr))
            failed += not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
