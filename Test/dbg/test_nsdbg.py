#!/usr/bin/env python3
"""
Debugging a package in its decompiled source (plan step 9.1).

  newtc -hello -opkg hello.pkg                  a package (the hello app)
  newtc -pkg hello.pkg -odecompile hello.ns     its source + hello.nsdbg
  newtc -pkg hello.pkg -nsdbg hello.nsdbg -dap  debug it

The package's code stays as it is; the debug map gives its functions line
tables into hello.ns. The DAP program calls the package's InstallScript;
a breakpoint on a line of hello.ns stops there, the stack frame shows
hello.ns, and "next" steps to the next line.

Usage: Test/dbg/test_nsdbg.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
import dap_client  # noqa: E402

PROGRAM = """// calls the package's InstallScript (ref0 is the package)
ref0.part[0].data:InstallScript({devInstallScript: func(x) nil});
Print("done");
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        run = lambda *a: subprocess.run([args.newtc, *a], cwd=tmp, capture_output=True, text=True)
        run("-hello", "-opkg", "hello.pkg")
        run("-pkg", "hello.pkg", "-odecompile", "hello.ns")
        source = (tmp / "hello.ns").read_text().splitlines()
        dbg = json.loads((tmp / "hello.nsdbg").read_text())
        checks.append(("-odecompile writes the source and the map",
                       len(source) > 10 and len(dbg["functions"]) == 2))
        loaded = run("-pkg", "hello.pkg", "-nsdbg", "hello.nsdbg")
        checks.append(("-nsdbg finds the functions", "2 of 2 functions found" in loaded.stderr))

        # the line of "if HasSlot(...)" in InstallScript, and the next one
        if_line = next(i for i, text in enumerate(source, 1) if "if HasSlot(" in text)
        (tmp / "call.ns").write_text(PROGRAM)
        script = "\n".join([
            '{"command": "initialize", "arguments": {"clientID": "test", "adapterID": "newtonscript"}}',
            'wait initialized',
            '{"command": "launch", "arguments": {"program": "$DIR/call.ns"}}',
            '{"command": "setBreakpoints", "arguments": {"source": {"path": "$DIR/hello.ns"}, "breakpoints": [{"line": %d}]}}' % if_line,
            '{"command": "configurationDone"}',
            'wait stopped',
            '{"command": "stackTrace", "arguments": {"threadId": 1, "levels": 1}}',
            '{"command": "next", "arguments": {"threadId": 1}}',
            'wait stopped',
            '{"command": "stackTrace", "arguments": {"threadId": 1, "levels": 1}}',
            '{"command": "continue", "arguments": {"threadId": 1}}',
            'wait terminated',
            '{"command": "disconnect"}'])
        transcript, err, code = dap_client.run_script(
            args.newtc, script, tmp, {"DIR": str(tmp)}, ["-pkg", "hello.pkg", "-nsdbg", "hello.nsdbg"])
        frames = [json.loads(line[3:])["body"]["stackFrames"][0] for line in transcript.splitlines()
                  if line.startswith("<- ") and '"command":"stackTrace"' in line]
        checks.append(("the breakpoint in hello.ns is verified", '"verified":true,"line":%d' % if_line in transcript))
        checks.append(("it stops in InstallScript, at that line of hello.ns",
                       len(frames) > 0 and frames[0]["source"].get("name") == "hello.ns"
                       and frames[0]["line"] == if_line))
        checks.append(("next goes to the next line", len(frames) > 1 and frames[1]["line"] == if_line + 1))
        checks.append(("the program ends normally", code == 0 and '"done' in transcript))

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n--- transcript ---\n" + transcript + "\n--- stderr ---\n" + err)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
