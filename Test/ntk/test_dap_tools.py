#!/usr/bin/env python3
"""
newtc's own debug tools (plan step 3.18c): a Newton without NS Debug Tools
(`newtc -ntk-device` without -dbg, a stock Newton) gets newtc's package
"DebugTools:newtc" (DebugToolsPackage.ns: NSDT's NewtonScript, our natives);
newtc runs those natives as their C++ stand-ins (gBinCFunctionHook), so the
whole package is tried here; the ARM code itself is tried on Einstein
(Test/ntk/natives_test.ns).

  - the session uploads the tools (after the agent); the Newton has
    |DebugTools:newtc:Version| and NSDT's globals
  - a breakpoint stops (source and line), BreakLoop() in a function stops
    there: the stack, its variables, a step, continue
  - a second session on the same Newton: the tools aren't uploaded again

Usage: Test/ntk/test_dap_tools.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(REPO / "Test" / "dbg"))
import dap_client  # noqa: E402
from test_dap_remote import PROGRAM, Session  # noqa: E402
from test_mnp import free_port  # noqa: E402


def stack(s):
    r = s.request("stackTrace", {"threadId": 1})
    return [(f["name"], f["line"]) for f in r["body"]["stackFrames"]] if r["success"] else []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []
    transcripts = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    with tempfile.TemporaryDirectory() as tmp:
        program = Path(tmp) / "remote.ns"
        program.write_text(PROGRAM)
        port = free_port()
        env = dict(os.environ, NEWTC_NTK_DEVICE_SESSIONS="2")
        env.pop("NEWTC_NSDT_PACKAGE", None)
        # a stock Newton: no -dbg, no NS Debug Tools
        device = subprocess.Popen([args.newtc, "-ntk-device", "tcp-client:%d" % port],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)

        s = Session(args.newtc, tmp, port=port, device=device)
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(program), "target": "tcp:%d" % port})
            s.request("setBreakpoints", {"source": {"path": str(program)}, "breakpoints": [{"line": 3}]})
            s.request("setExceptionBreakpoints", {"filters": []})
            s.request("configurationDone")
            stopped = s.event("stopped")
            console = s.output_text("console")
            check("a stock Newton gets newtc's debug tools (after the agent)",
                  "Uploading DAPAgent:newtc" in console and "Uploading DebugTools:newtc" in console
                  and console.find("Uploading DAPAgent:newtc") < console.find("Uploading DebugTools:newtc"))
            check("a breakpoint stops there (line 3, top level)",
                  stopped["body"]["reason"] == "breakpoint" and stack(s)[:1] == [("<program>", 3)])
            r = s.request("evaluate", {"expression": "[HasPath(functions, '|DebugTools:newtc:Version|), "
                                       "HasPath(functions, 'NSDOriginalBreakLoop), "
                                       "HasPath(functions, 'GetAllNamedVars)]", "context": "repl"})
            check("... the Newton has our tools and NSDT's globals",
                  r["success"] and r["body"]["result"].replace(" ", "").startswith("[true,true,true]"))
            s.request("continue", {"threadId": 1})
            stopped = s.event("stopped")
            frames = stack(s)
            check("BreakLoop() in Inner stops there: the stack",
                  stopped["body"]["reason"] == "pause" and frames[:2] == [("Inner", 8), ("<program>", 11)])
            r = s.request("scopes", {"frameId": 1})
            scopes = {sc["name"]: sc["variablesReference"] for sc in r["body"]["scopes"]} if r["success"] else {}
            a = s.request("variables", {"variablesReference": scopes.get("Arguments", 0)})
            l = s.request("variables", {"variablesReference": scopes.get("Locals", 0)})
            args_ = [(v["name"], v["value"]) for v in a["body"]["variables"]] if a["success"] else []
            locals_ = {v["name"]: v["value"] for v in l["body"]["variables"]} if l["success"] else {}
            check("... its argument and locals (GetAllNamedVars)",
                  args_ == [("a", "42")] and locals_.get("b") == "84")
            check("... a step (Step, the instruction decoder)",
                  s.request("next", {"threadId": 1})["success"]
                  and s.event("stopped")["body"]["reason"] == "step" and stack(s)[:1] == [("Inner", 9)])
            s.request("continue", {"threadId": 1})
            s.event("stopped")              # obj:M's BreakLoop()
            s.request("continue", {"threadId": 1})
            s.event("terminated")
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("session 1: no DAP errors (%s)" % e, False)
        s.close()
        transcripts.append("\n".join(s.client.lines))

        s = Session(args.newtc, tmp, port=port, device=device)
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(program), "target": "tcp:%d" % port})
            s.request("setExceptionBreakpoints", {"filters": []})
            s.request("configurationDone")
            s.event("stopped")
            check("a second session: the tools are there, not uploaded again",
                  "Uploading DebugTools:newtc" not in s.output_text("console"))
            s.request("continue", {"threadId": 1})
            s.event("stopped")
            s.request("continue", {"threadId": 1})
            s.event("terminated")
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("session 2: no DAP errors (%s)" % e, False)
        s.close()
        transcripts.append("\n".join(s.client.lines))
        try:
            device.wait(timeout=10)
        except subprocess.TimeoutExpired:
            device.kill()
            device.wait()
        logs = device.stderr.read() + device.stdout.read()
        check("the device installed the tools once", logs.count('installed "DebugTools:newtc"') == 1)

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n\n".join(t[-5000:] for t in transcripts) + "\n--- device ---\n" + logs[-3000:])
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
