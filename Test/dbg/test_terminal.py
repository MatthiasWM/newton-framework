#!/usr/bin/env python3
"""
Line editing and history in the break loop (plan step 4.3).

Only active when stdin and stdout are a terminal and newtc was built with
libedit, so the .ns test cases (which pipe stdin) can't cover it. This test
runs newtc in a pseudo-terminal, types a command, recalls it with the up
arrow, and checks the prompt, the repeated output, and the history file
(in a temporary HOME, so the real ~/.newtc_history is not touched).

Usage: Test/dbg/test_terminal.py [--newtc path]
Exit code 0 if it passes, 1 if not, 2 if newtc was built without libedit.
"""

import argparse
import os
import pty
import re
import select
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def read_until(fd, marker, timeout=10.0):
    """Read from the pty until `marker` (bytes) shows up; return all output."""
    out = b""
    end = time.time() + timeout
    while marker not in out and time.time() < end:
        ready, _, _ = select.select([fd], [], [], 0.1)
        if ready:
            try:
                data = os.read(fd, 4096)
            except OSError:
                break
            if not data:
                break
            out += data
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as home:
        env = dict(os.environ, HOME=home, TERM="xterm")
        master, slave = pty.openpty()
        proc = subprocess.Popen(
            [args.newtc, "-s", 'BreakLoop(); Print("done");'],
            stdin=slave, stdout=slave, stderr=slave, env=env, close_fds=True)
        os.close(slave)

        out = read_until(master, b"(newtc) ")
        if b"(newtc) " not in out:
            print("no line-editing prompt; newtc built without libedit?")
            proc.kill()
            return 2
        os.write(master, b"Print(40 + 2);\r")
        out += read_until(master, b"(newtc) ")
        os.write(master, b"\x1b[A\r")              # up arrow, Enter
        out += read_until(master, b"(newtc) ")
        os.write(master, b"ExitBreakLoop();\r")
        out += read_until(master, b"done")
        proc.wait(timeout=10)
        os.close(master)

        text = out.decode("utf-8", "replace")
        history = (Path(home) / ".newtc_history")
        history_text = history.read_text() if history.exists() else ""
        # libedit escapes characters in its history file, e.g. a space as \040
        history_text = re.sub(r"\\([0-7]{3})", lambda m: chr(int(m.group(1), 8)), history_text)

        checks = [
            ("prompt shown", "(newtc) " in text),
            ("command ran twice (history)", text.count("42") >= 2),
            ("script continued", "done" in text),
            ("history file written", "Print(40 + 2);" in history_text),
            ("no duplicate history entry", history_text.count("Print(40 + 2);") == 1),
        ]
        failed = [name for name, ok in checks if not ok]
        for name, ok in checks:
            print(("ok      " if ok else "FAIL    ") + name)
        if failed:
            print("\n--- terminal output ---\n" + text)
            return 1
        return 0


if __name__ == "__main__":
    sys.exit(main())
