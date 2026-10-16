#!/usr/bin/env python3
"""
The newton_ncx library (NCX's step 0.3): runs newton_ncx_test, which links
only the library, initializes it with InitObjectSystemCore() and exercises
what NCX uses of the framework (Test/ncx/newton_ncx_test.cc).

Usage: Test/ncx/test_newton_ncx.py [--test path to newton_ncx_test]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--test", default=str(REPO / "build" / "VSCode" / "newton_ncx_test"))
    args = ap.parse_args()
    run = subprocess.run([args.test, str(REPO / "ROMData" / "NTKFunctions.newtonpkg")],
                         capture_output=True, text=True, timeout=120)
    failed = 0
    for line in run.stdout.splitlines():
        if line.startswith(("ok ", "FAIL ")):
            print(line)
            failed += line.startswith("FAIL")
    clean = "Sanitizer" not in run.stderr and "Unhandled" not in run.stdout + run.stderr
    print(("ok      " if run.returncode == 0 and clean else "FAIL    ") + "newton_ncx_test ends cleanly (exit %d)" % run.returncode)
    if run.returncode != 0 or not clean:
        print(run.stdout[-2000:] + run.stderr[-2000:])
        failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
