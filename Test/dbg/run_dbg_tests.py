#!/usr/bin/env python3
"""
Debugger test harness for newtc.

Every test case lives in Test/dbg/cases/ and consists of:

  <name>.ns        NewtonScript run with `newtc -script <name>.ns`
  <name>.in        (optional) lines fed to stdin, i.e. the commands typed
                   into the break loop; without it, stdin is empty
  <name>.expected  the expected output (stdout, then stderr if any)

Output is normalized before comparing: heap references printed as
`#<hex>` or `#0x<hex>` change from run to run, so any `#` followed by 6 or
more hex digits becomes `#<ref>`. Short immediates like `#2` (nil) are kept.
A lone carriage return (Newton line ending) is shown as `<CR>` plus a
newline, so the files stay readable and CR vs. LF is still visible.

Usage:
  Test/dbg/run_dbg_tests.py                 run all cases
  Test/dbg/run_dbg_tests.py stacktrace      run cases whose name contains "stacktrace"
  Test/dbg/run_dbg_tests.py -v <name>       also print the actual output
  Test/dbg/run_dbg_tests.py --update <name> (re)write <name>.expected from the actual output
"""

import argparse
import difflib
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CASES = HERE / "cases"
REPO = HERE.parent.parent
DEFAULT_NEWTC = REPO / "build" / "VSCode" / "newtc"
TIMEOUT = 10  # seconds; a break loop waiting for input would otherwise hang

REF_RE = re.compile(r"#(0x)?[0-9A-Fa-f]{6,}")


def normalize(text):
    text = text.replace("\r\n", "\n").replace("\r", "<CR>\n")
    return REF_RE.sub("#<ref>", text)


def run_case(newtc, ns):
    inp = ns.with_suffix(".in")
    stdin = inp.read_bytes() if inp.exists() else b""
    try:
        proc = subprocess.run(
            [str(newtc), "-script", ns.name],
            cwd=ns.parent, input=stdin, capture_output=True, timeout=TIMEOUT)
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode("utf-8", "replace")
        return normalize(out) + f"\n--- TIMEOUT after {TIMEOUT}s ---\n"
    out = proc.stdout.decode("utf-8", "replace")
    err = proc.stderr.decode("utf-8", "replace")
    if err:
        out += "\n--- stderr ---\n" + err
    if proc.returncode != 0:
        out += f"\n--- exit code {proc.returncode} ---\n"
    return normalize(out)


def main():
    ap = argparse.ArgumentParser(description="Run newtc debugger tests.")
    ap.add_argument("names", nargs="*", help="only run cases whose name contains one of these")
    ap.add_argument("--newtc", default=str(DEFAULT_NEWTC), help="path to the newtc binary")
    ap.add_argument("--update", action="store_true", help="write .expected files from actual output")
    ap.add_argument("-v", "--verbose", action="store_true", help="print actual output")
    args = ap.parse_args()

    newtc = Path(args.newtc)
    if not newtc.exists():
        sys.exit(f"newtc not found: {newtc}")

    cases = sorted(CASES.glob("*.ns"))
    if args.names:
        cases = [c for c in cases if any(n in c.stem for n in args.names)]
    if not cases:
        sys.exit("no matching test cases")

    failed = 0
    for ns in cases:
        actual = run_case(newtc, ns)
        exp_file = ns.with_suffix(".expected")
        if args.verbose:
            print(f"----- {ns.stem} -----\n{actual}")
        if args.update:
            exp_file.write_text(actual)
            print(f"UPDATED {ns.stem}")
            continue
        if not exp_file.exists():
            print(f"MISSING {ns.stem}  (run with --update to create {exp_file.name})")
            failed += 1
            continue
        expected = exp_file.read_text()
        if actual == expected:
            print(f"ok      {ns.stem}")
        else:
            failed += 1
            print(f"FAIL    {ns.stem}")
            sys.stdout.writelines(difflib.unified_diff(
                expected.splitlines(keepends=True), actual.splitlines(keepends=True),
                fromfile=f"{ns.stem}.expected", tofile=f"{ns.stem} (actual)"))

    if not args.update:
        print(f"\n{len(cases) - failed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
