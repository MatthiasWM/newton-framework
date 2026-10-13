#!/usr/bin/env python3
"""
Breakpoints in InstallScripts (plan step 3.15): an InstallScript runs while
its package installs, before newtc can set breakpoints in the installed
package. With a breakpoint in one, or "Break in InstallScript", newtc
uploads the package with its install functions wrapped, and the agent sets
the breakpoints in the function that runs (DAPRemote.ns, Agent.ns v12).
The Newton is `newtc -dbg -ntk-device` (the ROM's InstallPart: a form
part's install function runs as an EnsureInternal copy).

  - an auto part compiled with -g: a breakpoint in its InstallScript
    stops there (source, line, a variable), a step, continue: installed
  - a form part, "Break in InstallScript": stops at its first line
    (reason "entry"), in the copy the ROM runs
  - the same form part decompiled (debug map): a breakpoint in the
    decompiled InstallScript stops there, the stack shows that line
  - no breakpoint in an InstallScript, no filter: uploaded unchanged

Usage: Test/ntk/test_dap_install.py [--newtc path]
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
from test_dap_remote import Session  # noqa: E402

AUTO = '''{
  signature: 'package0, id: "xxxx", flags: {noCompression: true}, version: 1,
  copyright: "test", name: "InstAuto:TEST", modifyDate: 0, info: "test",
  part: [{
    offset: 0, size: 0, type: "auto", flags: {type: 'nos, Notify: true}, info: "test",
    data: {
      InstallScript: func(partFrame)
      begin
        local a := 6;
        local b := a * 7;
        DefGlobalVar('instAutoResult, b);
      end
    }
  }]
}
'''
AUTO_BP_LINE = 10       # local b := a * 7;

FORM = '''{
  signature: 'package0, id: "xxxx", flags: {noCompression: true}, version: 1,
  copyright: "test", name: "InstForm:TEST", modifyDate: 0, info: "test",
  part: [{
    offset: 0, size: 0, type: "form", flags: {type: 'nos, Notify: true}, info: "test",
    data: {
      app: '|InstForm:TEST|,
      text: "InstForm",
      theForm: {_proto: @180, viewBounds: {left: 0, top: 50, right: 200, bottom: 120}},
      InstallScript: func(partFrame)
      begin
        local c := 20;
        DefGlobalVar('instFormResult, c + 22);
      end
    }
  }]
}
'''
FORM_FIRST_LINE = 12    # local c := 20;


def stack(s):
    r = s.request("stackTrace", {"threadId": 1})
    return r["body"]["stackFrames"] if r["success"] else []


def wait_installed(s, name):
    while ("Installed %s on the Newton." % name) not in s.output_text("console") \
            and ("couldn't install %s" % name) not in s.output_text("console"):
        s.event("output")
    return ("Installed %s on the Newton." % name) in s.output_text("console")


def evaluate(s, expression, frame_id=None):
    arguments = {"expression": expression, "context": "repl"}
    if frame_id:
        arguments["frameId"] = frame_id
    r = s.request("evaluate", arguments)
    return r["body"]["result"] if r["success"] else "error: " + r.get("message", "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []
    transcripts = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    with tempfile.TemporaryDirectory() as tmp:
        auto = Path(tmp) / "auto.ns"
        auto.write_text(AUTO)
        auto_pkg = Path(tmp) / "auto.pkg"
        subprocess.run([args.newtc, "-g", "-script", str(auto), "-opkg", str(auto_pkg)],
                       check=True, capture_output=True, stdin=subprocess.DEVNULL)
        form = Path(tmp) / "form.ns"
        form.write_text(FORM)
        form_pkg = Path(tmp) / "form.pkg"
        subprocess.run([args.newtc, "-g", "-script", str(form), "-opkg", str(form_pkg)],
                       check=True, capture_output=True, stdin=subprocess.DEVNULL)

        # 1. an auto part: a breakpoint in its InstallScript
        s = Session(args.newtc, tmp, ["-dbg"])
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(auto_pkg), "target": "tcp:%d" % s.port})
            s.request("setBreakpoints", {"source": {"path": str(auto)}, "breakpoints": [{"line": AUTO_BP_LINE}]})
            s.request("configurationDone")
            stopped = s.event("stopped")
            frames = stack(s)
            check("auto part: uploaded with its InstallScript wrapped",
                  "Uploading InstAuto:TEST with its InstallScript wrapped" in s.output_text("console"))
            check("auto part: a breakpoint in the InstallScript stops there, in its source",
                  stopped["body"]["reason"] == "breakpoint" and frames[:1]
                  and (frames[0].get("source", {}).get("path"), frames[0]["line"]) == (str(auto), AUTO_BP_LINE))
            check("... its variable a", evaluate(s, "a", 1) == "6")
            check("... step to the next line", s.request("next", {"threadId": 1})["success"]
                  and s.event("stopped")["body"]["reason"] == "step"
                  and stack(s)[0]["line"] == AUTO_BP_LINE + 1)
            s.request("continue", {"threadId": 1})
            check("... continue: installed, the InstallScript did its work",
                  wait_installed(s, "InstAuto:TEST") and evaluate(s, "instAutoResult") == "42")
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("auto part: no DAP errors (%s)" % e, False)
        logs = s.close()
        transcripts.append("\n".join(s.client.lines) + "\n--- logs ---\n" + logs)

        # 2. a form part: "Break in InstallScript"
        s = Session(args.newtc, tmp, ["-dbg"])
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            r = s.request("launch", {"program": str(form_pkg), "target": "tcp:%d" % s.port})
            s.request("setExceptionBreakpoints", {"filters": ["installScript"]})
            s.request("configurationDone")
            stopped = s.event("stopped")
            frames = stack(s)
            check("form part, Break in InstallScript: stops at its first line (reason entry)",
                  stopped["body"]["reason"] == "entry" and frames[:1]
                  and (frames[0].get("source", {}).get("path"), frames[0]["line"]) == (str(form), FORM_FIRST_LINE))
            s.request("continue", {"threadId": 1})
            check("... continue: installed (stops once)",
                  wait_installed(s, "InstForm:TEST") and evaluate(s, "instFormResult") == "42")
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("form part: no DAP errors (%s)" % e, False)
        logs = s.close()
        transcripts.append("\n".join(s.client.lines) + "\n--- logs ---\n" + logs)

        # 3. the form part decompiled: a breakpoint in the decompiled InstallScript
        plain_pkg = Path(tmp) / "plain.pkg"
        subprocess.run([args.newtc, "-script", str(form), "-opkg", str(plain_pkg)],
                       check=True, capture_output=True, stdin=subprocess.DEVNULL)
        dec = Path(tmp) / "plain_dec.ns"
        subprocess.run([args.newtc, "-pkg", str(plain_pkg), "-odecompile", str(dec)],
                       check=True, capture_output=True, stdin=subprocess.DEVNULL)
        dec_line = next(i + 1 for i, line in enumerate(dec.read_text().splitlines()) if "+ 22" in line)
        s = Session(args.newtc, tmp, ["-dbg"])
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(plain_pkg), "target": "tcp:%d" % s.port,
                                 "debugMap": str(dec.with_suffix(".nsdbg"))})
            s.request("setBreakpoints", {"source": {"path": str(dec)}, "breakpoints": [{"line": dec_line}]})
            s.request("configurationDone")
            stopped = s.event("stopped")
            frames = stack(s)
            check("decompiled form part: a breakpoint in the decompiled InstallScript stops there "
                  "(the ROM's copy is known to the agent)",
                  stopped["body"]["reason"] == "breakpoint" and frames[:1]
                  and (os.path.realpath(frames[0].get("source", {}).get("path", "")), frames[0]["line"])
                  == (os.path.realpath(dec), dec_line))
            s.request("continue", {"threadId": 1})
            check("... continue: installed", wait_installed(s, "InstForm:TEST"))
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("decompiled form part: no DAP errors (%s)" % e, False)
        logs = s.close()
        transcripts.append("\n".join(s.client.lines) + "\n--- logs ---\n" + logs)

        # 4. nothing in an InstallScript: the package as it is
        s = Session(args.newtc, tmp, ["-dbg"])
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(auto_pkg), "target": "tcp:%d" % s.port})
            s.request("configurationDone")
            check("no InstallScript breakpoint: installed, not wrapped",
                  wait_installed(s, "InstAuto:TEST")
                  and "wrapped" not in s.output_text("console"))
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("unwrapped: no DAP errors (%s)" % e, False)
        logs = s.close()
        transcripts.append("\n".join(s.client.lines) + "\n--- logs ---\n" + logs)

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n\n".join(t[-5000:] for t in transcripts))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
