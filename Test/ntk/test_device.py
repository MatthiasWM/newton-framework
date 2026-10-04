#!/usr/bin/env python3
"""
newtc playing the Newton (plan step 3.4a, Matt/NTKDevice.{h,cc}): the
terminal Inspector `newtc -ntk tcp:<port>` (the desktop) and
`newtc -ntk-device tcp-client:<port>` (the Newton, connecting like
Einstein) talk to each other through MNP and the Toolkit packets:

  - the MNP link with newtc in the Newton's role (LR first), 'cnnt',
    'okln', Toolkit.pkg's 'dante' object
  - 'lscb': the device's REP runs the code block and prints the result
    ("#14       5"), Print's text arrives
  - 'code': results come back as objects (numbers, strings, symbols)
  - exceptions as 'eref' (Undefined global function) and 'eerr' (Not in a
    break loop), printed on the desktop like newtc's REPL prints them
  - 'term' from the desktop ends the device
  - an exception inside a 'code' block ends the connection, as on a Newton
  - debugging (3.4b): with NS Debug Tools on the device (-dbg) and names
    (-g on the desktop), a breakpoint, inspection with 'code' while
    stopped, Step, 'fstk', ExitBreakLoop: the session of Toolkit
    Protocol.md 7.1, as on Einstein
  - packages (3.4c): 'pkg ' installs without opening, the same again gives
    -10402, 'pkgX' removes

Usage: Test/ntk/test_device.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import subprocess
import tempfile
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
from test_mnp import free_port  # noqa: E402


def session(newtc, lines, timeout=20, desk_args=(), dev_args=()):
    """Run desktop and device; return (desktop stdout, device stderr, codes)."""
    port = free_port()
    desk = subprocess.Popen([newtc] + list(desk_args) + ["-ntk", "tcp:%d" % port], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    # until the desktop listens
    deadline = time.time() + 5
    first = desk.stdout.readline()
    dev = subprocess.Popen([newtc] + list(dev_args) + ["-ntk-device", "tcp-client:%d" % port],
                           stdin=subprocess.DEVNULL,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        out, err = desk.communicate("".join(l + "\n" for l in lines), timeout=timeout)
    except subprocess.TimeoutExpired:
        desk.kill()
        out, err = desk.communicate()
    try:
        dev_out, dev_err = dev.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        dev.kill()
        dev_out, dev_err = dev.communicate()
    return first + out, err + dev_out + dev_err, desk.returncode, dev.returncode


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    out, logs, desk_code, dev_code = session(args.newtc, [
        "breakOnThrows := nil",
        "2+3",
        "= 6*7",
        'Print("Hello from the device")',
        "= [3, \"x3y\", 'sym]",
        "NoSuchFunctionProbe()",
        "ExitBreakLoop()",
    ])
    lines = out.splitlines()
    check("the desktop sees the device connect", any(l.startswith("ntk: connected") for l in lines))
    check("Toolkit.pkg's 'dante' object", any(l.startswith("ntk: fobj {interpretation: 'dante") for l in lines))
    check("lscb: the REP's result", "#14       5" in lines)
    check("code: the result as an object", "42" in lines)
    check("Print's text", '"Hello from the device"' in lines)
    check("code: an array of number, string, symbol", "[3, \"x3y\", 'sym]" in lines)
    check("eref: Undefined global function", "    !!! Exception: Undefined global function" in lines)
    check("eerr: Not in a break loop", "    !!! Exception: Not in a break loop" in lines)
    check("the desktop ends normally", lines[-1:] == ["ntk: done"] and desk_code == 0)
    check("the device ends when the desktop sends term",
          "ntk-device: the desktop ended the connection" in logs and dev_code == 0)
    check("no sanitizer reports", "Sanitizer" not in logs and "runtime error" not in logs)

    # an exception in a code block ends the connection, as on a Newton
    out, logs, desk_code, dev_code = session(args.newtc, ["= NoSuchFunctionProbe()", "2+3"])
    lines = out.splitlines()
    check("an exception in 'code': the device ends the connection",
          "ntk-device: an exception in a 'code' block ends the connection (as on a Newton)" in logs)
    check("... the desktop sees it", any(l.startswith("ntk: disconnected") for l in lines))
    check("... and both end", desk_code == 0 and dev_code == 0)

    # debugging on the device (3.4b): NS Debug Tools (-dbg), names (-g),
    # the session of Toolkit Protocol.md 7.1 as on Einstein
    out, logs, desk_code, dev_code = session(args.newtc, [
        "breakOnThrows := nil",
        "= DefGlobalFn('ProbeAdd, func(a, b) begin local c := a + b; c * 2 end)",
        "= InstallBreakPoint(functions.ProbeAdd, 4)",
        "= GloballyEnableBreakPoints(true)",
        "ProbeAdd(1, 2)",
        "= [GetCurrentPC(0), GetAllNamedVars(0), GetCurrentFunction(0) = functions.ProbeAdd]",
        "Step()",
        "= GetCurrentPC(0)",
        "StackTraceOld()",
        "= RemoveAllBreakPoints()",
        "ExitBreakLoop()",
        "= GloballyEnableBreakPoints(nil)",
    ], desk_args=["-g"], dev_args=["-dbg"])
    text = out.replace("\n ", "")        # pretty-printed objects on one line
    lines = text.splitlines()
    check("debug: the breakpoint stops: NSDT's location, eext",
          "functions.ProbeAdd('a=1, 'b=2), 4: GetVar c" in lines and "ntk: break loop entered" in lines)
    check("debug: code while stopped: PC, named variables, the function",
          "[4, {a: 1, b: 2, c: 3}, true]" in lines)
    check("debug: Step: bext, then the next stop", "ntk: break loop left" in lines
          and "functions.ProbeAdd('a=1, 'b=2), 5: PushConstant 2" in lines and "5" in lines)
    fstk = [l for l in lines if l.startswith("ntk: fstk")]
    check("debug: fstk as on Einstein (newest first, names, pc -1 for natives)",
          fstk and '{class: \'StackFrameInfoFrame, CodeBlock: "functions.StackTraceOld", programCounter: -1, '
                   'receiver: nil, contextFrame: nil}' in fstk[0]
          and 'CodeBlock: "functions.BreakLoop", programCounter: 409' in fstk[0]
          and 'CodeBlock: "functions.ProbeAdd", programCounter: 5' in fstk[0])
    check("debug: ExitBreakLoop: the call returns 6", "#18       6" in lines)
    check("debug: both end", desk_code == 0 and dev_code == 0 and lines[-1:] == ["ntk: done"])
    check("debug: no sanitizer reports", "Sanitizer" not in logs and "runtime error" not in logs)

    # packages (3.4c): 'pkg ' installs (InstallPart: the app's base view is
    # in the root, not opened), again: -10402; 'pkgX' removes (RemovePart)
    with tempfile.TemporaryDirectory() as tmp:
        pkg = str(Path(tmp) / "hello.pkg")
        subprocess.run([args.newtc, "-hello", "-opkg", pkg], check=True, capture_output=True)
        out, logs, desk_code, dev_code = session(args.newtc, [
            "= HasSlot(GetRoot(), '|hello:SIG|)",
            ":pkg " + pkg,
            "= [HasSlot(GetRoot(), '|hello:SIG|), GetRoot().|hello:SIG|.viewCObject <> nil]",
            ":pkg " + pkg,
            ":pkgx hello:SIG",
            "= HasSlot(GetRoot(), '|hello:SIG|)",
            ":pkgx hello:SIG",
        ])
    lines = out.replace("\n ", "").splitlines()
    results = [l for l in lines if not l.startswith("ntk:")]
    check("pkg: installed, its base view in the root, not opened",
          results[:2] == ["nil", "[true, nil]"] and 'ntk-device: installed "hello:SIG"' in logs)
    check("pkg: the same package again: -10402 (already exists)", "ntk: error -10402" in lines)
    check("pkgX: removed", results[2:3] == ["nil"] and 'ntk-device: removed "hello:SIG"' in logs)
    check("pkgX: a name that isn't there: no error", lines.count("ntk: error -10402") == 1
          and not any(l.startswith("ntk: error") and "-10402" not in l for l in lines))
    check("pkg: both end", desk_code == 0 and dev_code == 0)

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n--- desktop ---\n" + out + "\n--- logs ---\n" + logs)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
