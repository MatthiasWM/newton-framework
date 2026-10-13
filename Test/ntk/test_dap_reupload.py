#!/usr/bin/env python3
"""
No upload of an unchanged package (plan step 3.17). One Newton (`newtc
-dbg -ntk-device`) for several sessions; the agent (v13) remembers what
newtc uploaded (a hash of the bytes, with the size and timestamp the Newton
reports), and newtc skips the upload when the bytes are the same:

  1. launch: uploaded
  2. the same package again: not uploaded ("unchanged"); its breakpoints
     are still set (a call stops there)
  3. "reinstall": true: uploaded
  4. a rebuild (other bytes): uploaded, the new code runs
  5. "Break in InstallScript": uploaded (wrapped), even though unchanged
  6. without the filter: uploaded (the Newton has the wrapped bytes)
  7. once more: unchanged

Usage: Test/ntk/test_dap_reupload.py [--newtc path]
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
from test_dap_remote import APP, Session  # noqa: E402
from test_mnp import free_port  # noqa: E402

UNCHANGED = "is unchanged on the Newton: not uploaded again"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []
    transcripts = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    with tempfile.TemporaryDirectory() as tmp:
        app = Path(tmp) / "app.ns"
        app.write_text(APP)
        app_pkg = Path(tmp) / "app.pkg"
        subprocess.run([args.newtc, "-g", "-script", str(app), "-opkg", str(app_pkg)],
                       check=True, capture_output=True, stdin=subprocess.DEVNULL)
        app2 = Path(tmp) / "app2.ns"
        app2.write_text(APP.replace("n * 3", "n * 4"))
        app2_pkg = Path(tmp) / "app2.pkg"
        subprocess.run([args.newtc, "-g", "-script", str(app2), "-opkg", str(app2_pkg)],
                       check=True, capture_output=True, stdin=subprocess.DEVNULL)

        port = free_port()
        env = dict(os.environ, NEWTC_NTK_DEVICE_SESSIONS="7")
        device = subprocess.Popen([args.newtc, "-dbg", "-ntk-device", "tcp-client:%d" % port],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)

        def session(pkg, extra=None, filters=None, breakpoint_line=None, work=None):
            """One launch of pkg; its console text, and DebugeeWork(5)'s
            answer if work (a breakpoint at breakpoint_line stops it first:
            the stop's line)."""
            s = Session(args.newtc, tmp, port=port, device=device)
            result = {"console": "", "work": None, "stop": None}
            try:
                s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
                launch = {"program": str(pkg), "target": "tcp:%d" % port}
                launch.update(extra or {})
                s.request("launch", launch)
                if breakpoint_line:
                    s.request("setBreakpoints", {"source": {"path": str(app)},
                                                 "breakpoints": [{"line": breakpoint_line}]})
                if filters:
                    s.request("setExceptionBreakpoints", {"filters": filters})
                s.request("configurationDone")
                if filters:
                    s.event("stopped")
                    s.request("continue", {"threadId": 1})
                while "Installed Debugee:TEST" not in s.output_text("console") \
                        and UNCHANGED not in s.output_text("console"):
                    s.event("output")
                if work:
                    seq = s.client.send({"command": "evaluate",
                                         "arguments": {"expression": "DebugeeWork(5)", "context": "repl"}})
                    if breakpoint_line:
                        s.event("stopped")
                        r = s.request("stackTrace", {"threadId": 1})
                        result["stop"] = r["body"]["stackFrames"][0]["line"] if r["success"] else None
                        s.request("continue", {"threadId": 1})
                    while True:
                        m = s.client.receive()
                        if m is None:
                            break
                        if m.get("type") == "response" and m.get("request_seq") == seq:
                            result["work"] = m["body"]["result"] if m["success"] else None
                            break
                s.client.send({"command": "disconnect"})
            except dap_client.DAPError as e:
                check("no DAP errors (%s)" % e, False)
            s.close()
            result["console"] = s.output_text("console")
            transcripts.append("\n".join(s.client.lines))
            return result

        r = session(app_pkg)
        check("1. launch: uploaded", "Uploading app.pkg" in r["console"] and UNCHANGED not in r["console"])
        r = session(app_pkg, breakpoint_line=14, work=True)
        check("2. the same package: not uploaded again",
              UNCHANGED in r["console"] and "Uploading app.pkg" not in r["console"])
        check("... its breakpoints are set (a call stops there), its code runs",
              r["stop"] == 14 and r["work"] == "16")
        r = session(app_pkg, extra={"reinstall": True})
        check('3. "reinstall": true: uploaded', "Uploading app.pkg" in r["console"])
        r = session(app2_pkg, work=True)
        check("4. a rebuild: uploaded, the new code runs",
              "Uploading app2.pkg" in r["console"] and r["work"] == "21")
        r = session(app2_pkg, filters=["installScript"])
        check("5. Break in InstallScript: uploaded wrapped, though unchanged",
              "wrapped for debugging" in r["console"] and "Uploading app2.pkg" in r["console"])
        r = session(app2_pkg)
        check("6. without the filter: uploaded (the Newton has the wrapped bytes)",
              "Uploading app2.pkg" in r["console"])
        r = session(app2_pkg)
        check("7. once more: unchanged", UNCHANGED in r["console"])

        try:
            device.wait(timeout=10)
        except subprocess.TimeoutExpired:
            device.kill()
            device.wait()

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n\n".join(t[-4000:] for t in transcripts))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
