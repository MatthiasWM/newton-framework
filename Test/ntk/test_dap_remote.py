#!/usr/bin/env python3
"""
DAP with a remote Newton (plan step 3.6, Matt/Debugger/DAPRemote.ns):
`newtc -dap` gets a "launch" with "target", waits for the Newton (here
`newtc -ntk-device`), prepares it (breakOnThrows, our agent), and runs the
program there:

  - a .ns file: Print's output as "output" events, BreakLoop() in a
    function as "stopped" (pause), the stack from the agent (3.7a: names,
    file and line), evaluate while
    stopped (compiled here, run there; a frame result can be expanded; an
    error is an error response), "continue", an exception with "All
    Exceptions" on as "stopped" (exception, with its text); uncaught, it
    ends the program like -script does: "exited" and "terminated"
  - breakpoints answered as not yet available (plan step 3.8)
  - a .pkg: installed on the Newton (the agent first), the session goes on
    until the client disconnects
  - a target nobody can use: a message, then "terminated"

Usage: Test/ntk/test_dap_remote.py [--newtc path]
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
sys.path.insert(0, str(REPO / "Test" / "dbg"))
import dap_client  # noqa: E402
from test_mnp import free_port  # noqa: E402

PROGRAM = '''Print("hello from the Newton");
x := 6 * 7;
Print(x);
DefGlobalFn('Inner, func(a)
begin
  local b := a * 2;
  BreakLoop();
  b;
end);
Inner(x);
Print("after the break loop");
NoSuchFunctionProbe();
Print("end");
'''


class Session:
    """newtc -dap and newtc -ntk-device, and the DAP client."""

    def __init__(self, newtc, cwd, device_args=()):
        self.port = free_port()
        self.dap = subprocess.Popen([newtc, "-dap"], cwd=cwd, stdin=subprocess.PIPE,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.client = dap_client.Client(dap_client.ProcessConnection(self.dap))
        self.device = subprocess.Popen([newtc] + list(device_args) + ["-ntk-device", "tcp-client:%d" % self.port],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.outputs = []

    def request(self, command, arguments=None):
        message = {"command": command}
        if arguments is not None:
            message["arguments"] = arguments
        response = self.client.request(message)
        self._collect()
        return response

    def event(self, name):
        """Wait for an event; outputs on the way are kept."""
        while True:
            m = self.client.receive()
            if m is None:
                raise dap_client.DAPError("end of output while waiting for " + name)
            if m.get("type") == "event" and m.get("event") == name:
                return m

    def _collect(self):
        for line in self.client.lines:
            pass

    def output_text(self, category):
        """All "output" events of a category so far (from the transcript:
        request() reads events, too)."""
        text = ""
        for line in self.client.lines:
            if line.startswith("<- {"):
                m = json.loads(line[3:])
                if m.get("event") == "output" and m["body"]["category"] == category:
                    text += m["body"]["output"]
        return text

    def close(self):
        logs = ""
        for proc in (self.dap, self.device):
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        logs += self.dap.stderr.read().decode("utf-8", "replace")
        logs += self.device.stderr.read() + self.device.stdout.read()
        return logs


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

        # a .ns program; the device has NS Debug Tools (-dbg)
        s = Session(args.newtc, tmp, ["-dbg"])
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            r = s.request("launch", {"program": str(program), "target": "tcp:%d" % s.port})
            check("launch with a target", r["success"])
            r = s.request("setBreakpoints", {"source": {"path": str(program)}, "breakpoints": [{"line": 3}]})
            bp = r["body"]["breakpoints"][0]
            check("breakpoints: not yet on a remote Newton", not bp["verified"] and "3.8" in bp["message"])
            s.request("setExceptionBreakpoints", {"filters": ["all"]})
            s.request("configurationDone")
            stopped = s.event("stopped")
            console = s.output_text("console")
            check("waits for the Newton, says how to connect",
                  "Waiting for a Newton on tcp:%d" % s.port in console and "Connect Inspector" in console)
            check("the Newton connects", "The Newton is connected." in console)
            check("the program's output, from the Newton",
                  '"hello from the Newton"\n42\n' in s.output_text("stdout"))
            check("BreakLoop(): stopped (pause)", stopped["body"]["reason"] == "pause"
                  and stopped["body"]["description"] == "Paused in BreakLoop()")
            r = s.request("stackTrace", {"threadId": 1})
            frames = r["body"]["stackFrames"] if r["success"] else []
            where = [(f["name"], f.get("source", {}).get("path"), f["line"]) for f in frames]
            check("stackTrace (3.7a): Inner at its BreakLoop() line, <program> at the call",
                  where == [("Inner", str(program), 7), ("<program>", str(program), 10)])
            check("no NS Debug Tools location text in the output (the agent's stop hook)",
                  "PC may not be accurate" not in s.output_text("stdout")
                  and ": Return" not in s.output_text("stdout"))
            r = s.request("evaluate", {"expression": "x + 1", "context": "repl"})
            check("evaluate while stopped: run on the Newton", r["success"] and r["body"]["result"] == "43")
            r = s.request("evaluate", {"expression": "{a: x, b: [1, 2]}", "context": "repl"})
            ref = r["body"].get("variablesReference", 0) if r["success"] else 0
            check("evaluate: a frame can be expanded", ref > 0)
            if ref:
                v = s.request("variables", {"variablesReference": ref})
                names = [(x["name"], x["value"]) for x in v["body"]["variables"]]
                check("... its slots", ("a", "42") in names and ("b", "[1, 2]") in names)
            r = s.request("evaluate", {"expression": "NoSuchFunctionProbe()", "context": "repl"})
            check("evaluate: an error is an error response",
                  not r["success"] and "Undefined global function" in r["message"])
            r = s.request("continue", {"threadId": 1})
            check("continue", r["success"])
            stopped = s.event("stopped")
            check("an exception with All Exceptions: stopped (exception, with its text)",
                  stopped["body"]["reason"] == "exception"
                  and "Undefined global function" in stopped["body"].get("text", ""))
            check("... after the output before it", "after the break loop" in s.output_text("stdout"))
            check("... and as stderr output", "!!! Exception: Undefined global function" in s.output_text("stderr"))
            s.request("continue", {"threadId": 1})
            exited = s.event("exited")
            s.event("terminated")
            check("uncaught, it ends the program (like -script): exited (0: it stopped), terminated",
                  '"end"' not in s.output_text("stdout") and exited["body"]["exitCode"] == 0)
            check("no REP echo (of the program, of ExitBreakLoop)",
                  "#A8" not in s.output_text("stdout") and "#2 " not in s.output_text("stdout"))
            r = s.request("continue", {"threadId": 1})
            check("continue after the end: an error response", not r["success"])
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("no DAP errors (%s)" % e, False)
        logs = s.close()
        transcripts.append("\n".join(s.client.lines) + "\n--- logs ---\n" + logs)
        check("newtc -dap ends after disconnect", s.dap.returncode == 0)
        check("the device ends too", s.device.returncode == 0 and "the desktop ended the connection" in logs)
        check("no sanitizer reports", "Sanitizer" not in logs and "runtime error" not in logs)

        # a .pkg program; a stock device: NS Debug Tools are missing
        pkg = Path(tmp) / "hello.pkg"
        subprocess.run([args.newtc, "-hello", "-opkg", str(pkg)], check=True, capture_output=True)
        s = Session(args.newtc, tmp)
        pending_tools_check = False
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(pkg), "target": "tcp:%d" % s.port})
            s.request("configurationDone")
            while "Installed hello:SIG on the Newton." not in s.output_text("console"):
                s.event("output")
            check("a package: installed on the Newton", True)
            pending_tools_check = "Warning: NS Debug Tools: " in s.output_text("console")
            r = s.request("evaluate", {"expression": "HasSlot(GetRoot(), '|hello:SIG|)", "context": "repl"})
            check("... while it runs, evaluate is refused (the Newton only answers when idle)",
                  not r["success"] and "running" in r["message"])
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("package: no DAP errors (%s)" % e, False)
        logs = s.close()
        transcripts.append("\n".join(s.client.lines) + "\n--- logs ---\n" + logs)
        # NS Debug Tools were missing: Apple's package uploaded (here its
        # install script defines NSDOriginalBreakLoop before it reaches ARM
        # code), or, without NTK's folder, a warning
        check("package: NS Debug Tools uploaded when missing (or a warning)",
              'installed "NS Debug Tools:PIE"' in logs or pending_tools_check)
        check("package: the agent installed first, then the package",
              logs.find('installed "DAPAgent:newtc"') < logs.find('installed "hello:SIG"') != -1)
        check("package: both end", s.dap.returncode == 0 and s.device.returncode == 0)

        # a target nobody can use
        s = Session(args.newtc, tmp)
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(program), "target": "nonsense"})
            s.request("configurationDone")
            s.event("terminated")
            check("a bad target: a message, terminated",
                  "Can't use the target \"nonsense\"" in s.output_text("console"))
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("bad target: no DAP errors (%s)" % e, False)
        s.device.kill()
        s.close()

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n--- transcripts ---\n" + "\n=====\n".join(transcripts + ["\n".join(s.client.lines)]))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
