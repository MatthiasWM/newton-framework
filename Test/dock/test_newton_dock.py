#!/usr/bin/env python3
"""
The newton_dock library (NCX's step 0.6): runs newton_dock_test, which links
only the library and checks MNP's CRC and framing, the Dock's DES, and two
MNP links talking over TCP (Test/dock/newton_dock_test.cc).

Usage: Test/dock/test_newton_dock.py [--test path to newton_dock_test]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--test", default=str(REPO / "build" / "VSCode" / "newton_dock_test"))
    args = ap.parse_args()
    run = subprocess.run([args.test], capture_output=True, text=True, timeout=120)
    failed = 0
    for line in run.stdout.splitlines():
        if line.startswith(("ok ", "FAIL ")):
            print(line)
            failed += line.startswith("FAIL")
    clean = "Sanitizer" not in run.stderr
    print(("ok      " if run.returncode == 0 and clean else "FAIL    ") + "newton_dock_test ends cleanly (exit %d)" % run.returncode)
    if run.returncode != 0 or not clean:
        print(run.stdout[-2000:] + run.stderr[-2000:])
        failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
