#!/usr/bin/env python3
"""
B51: a stop in an app's script while the app opens on newtc's fake Newton
(-ntk-device), with what VS Code asks after every stop (stackTrace, scopes,
variables). The view's viewCObject is a 64-bit address in newtc; the agent's
description of self held it, and writing the reply threw ("doesn't fit a
Newton"): the break loop unwound, the app closed ("Goodbye"), and the call
that opened it never answered (continue: "isn't stopped", evaluate: "The
Newton is running the program"; found by VSNewt). Now:

  - the stop holds through stackTrace, scopes, variables (self shows)
  - evaluate while stopped, continue: the app opens
  - evaluate while the app runs (the Newton is idle)

Usage: Test/ntk/test_dap_app_stop.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(REPO / "Test" / "dbg"))
import dap_client  # noqa: E402
from test_dap_remote import Session  # noqa: E402

APP = '''newtAppName := "StopApp:TEST";
newt.theForm := {
  _proto: protoFloatNGo,
  viewBounds: {left: 0, top: 50, right: 200, bottom: 130},
  viewSetupDoneScript: func()
  begin
    local opened := "opened";
    Print(opened);
  end,
  viewQuitScript: func() Print("Goodbye")
};
'''
LINE = 8   # Print(opened);


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "StopApp.ns"
        source.write_text(APP)
        pkg = Path(tmp) / "StopApp.pkg"
        subprocess.run([args.newtc, "-g", "-build", str(source), "-opkg", str(pkg)],
                       check=True, capture_output=True, stdin=subprocess.DEVNULL)
        s = Session(args.newtc, tmp)
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(pkg), "target": "tcp:%d" % s.port})
            s.request("setBreakpoints", {"source": {"path": str(source)}, "breakpoints": [{"line": LINE}]})
            s.request("configurationDone")
            stopped = s.event("stopped")
            r = s.request("stackTrace", {"threadId": 1})
            frames = r["body"]["stackFrames"] if r["success"] else []
            check("stops in viewSetupDoneScript while the app opens",
                  stopped["body"]["reason"] == "breakpoint" and frames[:1]
                  and frames[0]["name"].endswith("viewSetupDoneScript") and frames[0]["line"] == LINE)
            r = s.request("scopes", {"frameId": 1})
            scopes = r["body"]["scopes"] if r["success"] else []
            values = {}
            for scope in scopes:
                v = s.request("variables", {"variablesReference": scope["variablesReference"]})
                if v["success"]:
                    values.update({x["name"]: x["value"] for x in v["body"]["variables"]})
            check("... scopes and variables: self (the view, its viewCObject) and opened",
                  values.get("opened") == '"opened"' and "viewCObject" in values.get("self", ""))
            r = s.request("evaluate", {"expression": "opened", "context": "repl", "frameId": 1})
            check("... evaluate in the stopped frame", r["success"] and r["body"]["result"] == '"opened"')
            check("... the app didn't close meanwhile", "Goodbye" not in s.output_text("stdout"))
            r = s.request("continue", {"threadId": 1})
            check("continue", r["success"])
            while "Opened StopApp:TEST." not in s.output_text("console"):
                s.event("output")
            r = s.request("evaluate", {"expression": "HasSlot(GetRoot(), '|StopApp:TEST|)", "context": "repl"})
            check("the app opened; evaluate while it runs", r["success"] and r["body"]["result"] == "true")
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("no DAP errors (%s)" % e, False)
        logs = s.close()
        transcript = "\n".join(s.client.lines)

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print(transcript[-5000:] + "\n--- device ---\n" + logs[-3000:])
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
