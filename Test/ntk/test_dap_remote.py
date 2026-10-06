#!/usr/bin/env python3
"""
DAP with a remote Newton (plan step 3.6, Matt/Debugger/DAPRemote.ns):
`newtc -dap` gets a "launch" with "target", waits for the Newton (here
`newtc -ntk-device`), prepares it (breakOnThrows, our agent), and runs the
program there:

  - a .ns file: Print's output as "output" events, BreakLoop() in a
    function as "stopped" (pause), the stack from the agent (3.7a: names,
    file and line), scopes and variables from the agent (3.7b: values
    described there, frames and arrays expanded on request, paged),
    evaluate in a stopped frame (3.7c: its variables, self; assignments
    stay) and globally, evaluate while
    stopped (compiled here, run there; a frame result can be expanded; an
    error is an error response), "continue", an exception with "All
    Exceptions" on as "stopped" (exception, with its text); uncaught, it
    ends the program like -script does: "exited" and "terminated"
  - breakpoints (3.8): set before the program is compiled (pending, then
    verified), at top level and in a function, added and cleared while
    stopped; a stop there has reason "breakpoint"
  - stepping (3.9): next over top-level statements and lines, stepIn into
    functions (also onto a breakpoint), next over BreakLoop() (it stops
    there), stepOut of a method, stepOut of the program (can't: a message,
    it continues)
  - a .pkg: installed on the Newton (the agent first), the session goes on
    until the client disconnects; evaluate works (the Newton is idle)
  - a package compiled from source with -g (3.10): its line tables found, a
    breakpoint in it set once it is installed; evaluate calls its function
    (the response waits): a stop at the breakpoint, with source, variables,
    a step; continue, then the evaluate's answer
  - a decompiled package (-odecompile, its .nsdbg): a breakpoint in the
    decompiled source, the stack's line from the map (the agent's Known)
  - an exception in a package's InstallScript stops there
  - a target nobody can use: a message, then "terminated"

Usage: Test/ntk/test_dap_remote.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import json
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
from test_mnp import free_port  # noqa: E402

PROGRAM = '''Print("hello from the Newton");
x := 6 * 7;
Print(x);
DefGlobalFn('Inner, func(a)
begin
  local b := a * 2;
  local f := {name: "Newton", list: [1, 2, {deep: true}], fn: func(y) y};
  BreakLoop();
  b;
end);
Print(Inner(x));
Print("after the break loop");
obj := {v: 5, M: func(k) begin BreakLoop(); v + k end};
Print(obj:M(1));
NoSuchFunctionProbe();
Print("end");
'''

# A package with an auto part: its InstallScript defines DebugeeWork (the
# package's function itself, so its breakpoints apply)
APP = '''{
  signature: 'package0, id: "xxxx", flags: {noCompression: true}, version: 1,
  copyright: "test", name: "Debugee:TEST", modifyDate: 0, info: "test",
  part: [{
    offset: 0, size: 0, type: "auto", flags: {type: 'nos, Notify: true}, info: "test",
    data: {
      InstallScript: func(partFrame)
      begin
        DefGlobalFn('DebugeeWork, partFrame.Work);
        EnsureInternal({removeScript: func(r) RemoveSlot(functions, 'DebugeeWork)});
      end,
      Work: func(n)
      begin
        local m := n * 3;
        m + 1;
      end
    }
  }]
}
'''

BAD = '''{
  signature: 'package0, id: "xxxx", flags: {noCompression: true}, version: 1,
  copyright: "test", name: "BadInstall:TEST", modifyDate: 0, info: "test",
  part: [{
    offset: 0, size: 0, type: "auto", flags: {type: 'nos, Notify: true}, info: "test",
    data: {
      InstallScript: func(partFrame)
      begin
        NoSuchInstallProbe();
      end
    }
  }]
}
'''


class Session:
    """newtc -dap and newtc -ntk-device, and the DAP client."""

    def __init__(self, newtc, cwd, device_args=(), port=None, device=None):
        """port, device: a device that is already there (several sessions)."""
        self.port = port if port else free_port()
        self.dap = subprocess.Popen([newtc, "-dap"], cwd=cwd, stdin=subprocess.PIPE,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.client = dap_client.Client(dap_client.ProcessConnection(self.dap))
        self.shared_device = device is not None
        self.device = device if device else subprocess.Popen(
            [newtc] + list(device_args) + ["-ntk-device", "tcp-client:%d" % self.port],
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
        """Wait for both to end (a shared device: only newtc -dap); their logs."""
        logs = ""
        for proc in (self.dap,) if self.shared_device else (self.dap, self.device):
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        logs += self.dap.stderr.read().decode("utf-8", "replace")
        if not self.shared_device:
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

        def evaluate(expression, frame_id):
            r = s.request("evaluate", {"expression": expression, "context": "watch", "frameId": frame_id})
            return r["body"]["result"] if r["success"] else "error: " + r.get("message", "")
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            r = s.request("launch", {"program": str(program), "target": "tcp:%d" % s.port})
            check("launch with a target", r["success"])
            r = s.request("setBreakpoints", {"source": {"path": str(program)},
                                             "breakpoints": [{"line": 3}, {"line": 6}]})
            bps = r["body"]["breakpoints"]
            check("breakpoints (3.8): set before the program is compiled, pending",
                  len(bps) == 2 and not bps[0]["verified"] and "compiled" in bps[0]["message"])
            s.request("setExceptionBreakpoints", {"filters": ["all"]})
            s.request("configurationDone")
            stopped = s.event("stopped")
            console = s.output_text("console")
            check("waits for the Newton, says how to connect",
                  "Waiting for a Newton on tcp:%d" % s.port in console and "Connect Inspector" in console)
            check("the Newton connects", "The Newton is connected." in console)
            changed = [json.loads(line[3:])["body"]["breakpoint"] for line in s.client.lines
                       if line.startswith("<- {") and '"event":"breakpoint"' in line]
            check("... compiled: both verified (breakpoint events)",
                  sorted((b["line"], b["verified"]) for b in changed) == [(3, True), (6, True)])
            r = s.request("stackTrace", {"threadId": 1})
            frames = r["body"]["stackFrames"] if r["success"] else []
            check("stopped at the breakpoint on line 3 (top level), before it runs",
                  stopped["body"]["reason"] == "breakpoint" and frames[:1]
                  and (frames[0]["name"], frames[0]["line"]) == ("<program>", 3)
                  and "42" not in s.output_text("stdout"))

            def step(command):
                """A step; the stop's reason and [(name, line), ...]."""
                r = s.request(command, {"threadId": 1})
                if not r["success"]:
                    return "error: " + r.get("message", ""), []
                stopped = s.event("stopped")
                r = s.request("stackTrace", {"threadId": 1})
                frames = r["body"]["stackFrames"] if r["success"] else []
                return stopped["body"]["reason"], [(f["name"], f["line"]) for f in frames]
            check("next (3.9): the next top-level statement",
                  step("next") == ("step", [("<program>", 4)]) and "42\n" in s.output_text("stdout"))
            check("... next: over the whole DefGlobalFn statement",
                  step("next") == ("step", [("<program>", 11)]))
            reason, where = step("stepIn")
            check("stepIn: Inner's first statement (line 6, where a breakpoint is, too)",
                  reason in ("step", "breakpoint") and where == [("Inner", 6), ("<program>", 11)])
            check("... its argument is there", evaluate("a", 1) == "42")
            check("next: line 7", step("next") == ("step", [("Inner", 7), ("<program>", 11)]))
            check("... line 8", step("next") == ("step", [("Inner", 8), ("<program>", 11)]))
            reason, where = step("next")
            stopped = {"body": {"reason": reason, "description": "Paused in BreakLoop()" if reason == "pause" else ""}}
            check("next over BreakLoop(): it stops there (pause), the step ends",
                  reason == "pause" and where == [("Inner", 8), ("<program>", 11)])
            check("the program's output, from the Newton",
                  '"hello from the Newton"\n42\n' in s.output_text("stdout"))
            check("BreakLoop(): stopped (pause)", stopped["body"]["reason"] == "pause"
                  and stopped["body"]["description"] == "Paused in BreakLoop()")
            r = s.request("stackTrace", {"threadId": 1})
            frames = r["body"]["stackFrames"] if r["success"] else []
            where = [(f["name"], f.get("source", {}).get("path"), f["line"]) for f in frames]
            check("stackTrace (3.7a): Inner at its BreakLoop() line, <program> at the call",
                  where == [("Inner", str(program), 8), ("<program>", str(program), 11)])
            r = s.request("scopes", {"frameId": frames[0]["id"] if frames else 1})
            scopes = {sc["name"]: sc["variablesReference"] for sc in r["body"]["scopes"]} if r["success"] else {}
            check("scopes (3.7b): Arguments and Locals of Inner",
                  "Arguments" in scopes and "Locals" in scopes)

            def variables(ref, **paging):
                arguments = {"variablesReference": ref}
                arguments.update(paging)
                v = s.request("variables", arguments)
                return {x["name"]: x for x in v["body"]["variables"]} if v["success"] else {}

            def shown(found):
                return {name: x["value"] for name, x in found.items()}
            arguments = variables(scopes.get("Arguments", 0))
            check("... Arguments: a = 42", shown(arguments) == {"a": "42"})
            local_vars = variables(scopes.get("Locals", 0))
            check("... Locals: b = 84, f with a preview",
                  shown(local_vars).get("b") == "84"
                  and shown(local_vars).get("f") == '{name: "Newton", list: [...], fn: <function>}'
                  and local_vars.get("f", {}).get("type") == "frame")
            f = variables(local_vars.get("f", {}).get("variablesReference", 0))
            check("... f expands (|DAPAgent:Expand|): its slots",
                  shown(f) == {"name": '"Newton"', "list": "[1, 2, {...}]", "fn": "<function, 1 arg>"}
                  and f["list"].get("indexedVariables") == 3 and f["fn"]["variablesReference"] == 0
                  and f["list"].get("evaluateName") == "f.list")
            elements = variables(f.get("list", {}).get("variablesReference", 0), start=1, count=2)
            check("... an array, paged (start 1, count 2)",
                  list(shown(elements)) == ["[1]", "[2]"] and shown(elements)["[1]"] == "2"
                  and shown(elements)["[2]"] == "{deep: true}")
            deep = variables(elements.get("[2]", {}).get("variablesReference", 0))
            check("... and the frame in it", shown(deep) == {"deep": "true"})
            r = s.request("scopes", {"frameId": frames[1]["id"] if len(frames) > 1 else 2})
            check("scopes of <program>: none (the wrapper's local and try values are hidden)",
                  r["success"] and r["body"]["scopes"] == [])
            check("no NS Debug Tools location text in the output (the agent's stop hook)",
                  "PC may not be accurate" not in s.output_text("stdout")
                  and ": Return" not in s.output_text("stdout"))
            r = s.request("evaluate", {"expression": "x + 1", "context": "repl"})
            check("evaluate while stopped: run on the Newton", r["success"] and r["body"]["result"] == "43")
            r = s.request("evaluate", {"expression": "{a: x, b: [1, 2, [3]]}", "context": "repl"})
            ref = r["body"].get("variablesReference", 0) if r["success"] else 0
            check("evaluate: a frame, described by the agent, can be expanded",
                  ref > 0 and r["body"]["result"] == "{a: 42, b: [...]}")
            if ref:
                v = s.request("variables", {"variablesReference": ref})
                names = [(x["name"], x["value"]) for x in v["body"]["variables"]]
                check("... its slots", ("a", "42") in names and ("b", "[1, 2, [...]]") in names)

            check("evaluate in Inner's frame (3.7c): its arguments and locals",
                  evaluate("a + b", 1) == "126" and evaluate("f.list[2].deep", 1) == "true")
            check("... a local frame, expandable",
                  evaluate("f", 1) == '{name: "Newton", list: [...], fn: <function>}')
            check("... an assignment changes the variable (Inner returns it)",
                  evaluate("b := b + 1000", 1) == "1084" and evaluate("b", 1) == "1084")
            check("... in <program>'s frame: globals", evaluate("x * 2", 2) == "84")
            check("... a syntax error: an error response", evaluate("a +", 1).startswith("error: "))
            r = s.request("evaluate", {"expression": "NoSuchFunctionProbe()", "context": "repl"})
            check("evaluate: an error is an error response",
                  not r["success"] and "Undefined global function" in r["message"])
            r = s.request("setBreakpoints", {"source": {"path": str(program)}, "breakpoints": [{"line": 12}]})
            bps = r["body"]["breakpoints"] if r["success"] else []
            check("a breakpoint set while stopped (line 12): verified", bps and bps[0]["verified"]
                  and bps[0]["line"] == 12)
            r = s.request("continue", {"threadId": 1})
            check("continue", r["success"])
            stopped = s.event("stopped")
            check("... Inner returned the changed b", "1084\n" in s.output_text("stdout"))
            r = s.request("stackTrace", {"threadId": 1})
            frames = r["body"]["stackFrames"] if r["success"] else []
            check("... it stops there; lines 3 and 6 no longer", stopped["body"]["reason"] == "breakpoint"
                  and frames[:1] and frames[0]["line"] == 12)
            r = s.request("setBreakpoints", {"source": {"path": str(program)}, "breakpoints": []})
            check("breakpoints cleared", r["success"] and r["body"]["breakpoints"] == [])
            check("next, next: lines 13, 14", step("next") == ("step", [("<program>", 13)])
                  and step("next") == ("step", [("<program>", 14)]))
            check("stepIn: M's first statement (no breakpoint there)",
                  step("stepIn") == ("step", [("self.M", 13), ("<program>", 14)]))
            s.request("continue", {"threadId": 1})
            stopped = s.event("stopped")
            r = s.request("stackTrace", {"threadId": 1})
            frames = r["body"]["stackFrames"] if r["success"] else []
            check("a method stops in BreakLoop(): self.M on the stack",
                  stopped["body"]["reason"] == "pause" and frames[:1] and frames[0]["name"] == "self.M"
                  and frames[0]["line"] == 13)
            r = s.request("scopes", {"frameId": 1})
            scopes = {sc["name"]: sc["variablesReference"] for sc in r["body"]["scopes"]} if r["success"] else {}
            local_vars = variables(scopes.get("Locals", 0))
            check("... Locals: self, expandable",
                  local_vars.get("self", {}).get("variablesReference", 0) > 0
                  and shown(variables(local_vars["self"]["variablesReference"])).get("v") == "5")
            check("... evaluate sees self's slots and k", evaluate("v * 10 + k", 1) == "51")
            check("... an assignment to a slot of self stays", evaluate("v := 7", 1) == "7"
                  and evaluate("obj.v", 1) == "7")
            check("stepOut of M: back in <program> on line 14, before Print",
                  step("stepOut") == ("step", [("<program>", 14)]) and "8\n" not in s.output_text("stdout"))
            r = s.request("stepOut", {"threadId": 1})
            stopped = s.event("stopped")
            check("stepOut of <program>: can't (native code called it), it continues",
                  r["success"] and "Can't step out" in s.output_text("console")
                  and "continuing." in s.output_text("console"))
            check("... M returned v + k with the new v", "8\n" in s.output_text("stdout"))
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
            while "Opened hello:SIG." not in s.output_text("console"):
                s.event("output")
            check("... its app opened (the ROM's install doesn't)", True)
            pending_tools_check = "Warning: NS Debug Tools: " in s.output_text("console")
            r = s.request("evaluate", {"expression": "HasSlot(GetRoot(), '|hello:SIG|)", "context": "repl"})
            check("... then the Newton is idle: evaluate works",
                  r["success"] and r["body"]["result"] == "true")
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
        check("package: both end (exit codes %s, %s)" % (s.dap.returncode, s.device.returncode),
              s.dap.returncode == 0 and s.device.returncode == 0)

        # a package compiled from source with -g, on a device with NS Debug Tools
        app = Path(tmp) / "app.ns"
        app.write_text(APP)
        app_pkg = Path(tmp) / "app.pkg"
        subprocess.run([args.newtc, "-g", "-script", str(app), "-opkg", str(app_pkg)],
                       check=True, capture_output=True)
        s = Session(args.newtc, tmp, ["-dbg"])
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(app_pkg), "target": "tcp:%d" % s.port})
            check("a -g package: its line tables are found",
                  "app.pkg: 3 functions with line tables" in s.output_text("console"))
            r = s.request("setBreakpoints", {"source": {"path": str(app)}, "breakpoints": [{"line": 14}]})
            bp = r["body"]["breakpoints"][0] if r["success"] else {}
            check("... a breakpoint in it: pending until the package is there",
                  not bp.get("verified") and "compiled" in bp.get("message", ""))
            s.request("configurationDone")
            while "Installed Debugee:TEST on the Newton." not in s.output_text("console"):
                s.event("output")
            changed = [json.loads(line[3:])["body"]["breakpoint"] for line in s.client.lines
                       if line.startswith("<- {") and '"event":"breakpoint"' in line]
            check("... verified when the session starts", [(b["line"], b["verified"]) for b in changed] == [(14, True)])
            seq = s.client.send({"command": "evaluate", "arguments": {"expression": "DebugeeWork(5)", "context": "repl"}})
            stopped = s.event("stopped")
            r = s.request("stackTrace", {"threadId": 1})
            frames = r["body"]["stackFrames"] if r["success"] else []
            check("evaluate calls the package's function (installed by its InstallScript): "
                  "it stops at the breakpoint, in the package's source",
                  stopped["body"]["reason"] == "breakpoint" and frames[:1]
                  and (frames[0]["name"], frames[0].get("source", {}).get("path"), frames[0]["line"])
                  == ("DebugeeWork", str(app), 14))
            r = s.request("scopes", {"frameId": 1})
            scopes = {sc["name"]: sc["variablesReference"] for sc in r["body"]["scopes"]} if r["success"] else {}
            v = s.request("variables", {"variablesReference": scopes.get("Arguments", 0)})
            check("... its argument", r["success"] and [(x["name"], x["value"]) for x in v["body"]["variables"]]
                  == [("n", "5")])
            check("... step", s.request("next", {"threadId": 1})["success"]
                  and s.event("stopped")["body"]["reason"] == "step")
            r = s.request("stackTrace", {"threadId": 1})
            check("... to line 15", r["success"] and r["body"]["stackFrames"][0]["line"] == 15)
            s.request("continue", {"threadId": 1})
            r = s.request("evaluate", {"expression": "1 + 1", "context": "repl"})
            while not any('"request_seq":%d,' % seq in line for line in s.client.lines):
                if s.client.receive() is None:
                    break
            answers = [json.loads(line[3:]) for line in s.client.lines if line.startswith("<- {")
                       and '"request_seq":%d,' % seq in line]
            check("... continue; an evaluate meanwhile is answered (2), then the first one (16)",
                  answers and answers[0]["success"] and answers[0]["body"]["result"] == "16"
                  and r["success"] and r["body"]["result"] == "2")
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("-g package: no DAP errors (%s)" % e, False)
        logs = s.close()
        transcripts.append("\n".join(s.client.lines) + "\n--- logs ---\n" + logs)
        check("-g package: both end", s.dap.returncode == 0 and s.device.returncode == 0)

        # the same package without -g, decompiled (-odecompile): debugged in the
        # decompiled source, with its debug map
        plain_pkg = Path(tmp) / "plain.pkg"
        subprocess.run([args.newtc, "-script", str(app), "-opkg", str(plain_pkg)], check=True, capture_output=True)
        dec = Path(tmp) / "plain_dec.ns"
        subprocess.run([args.newtc, "-pkg", str(plain_pkg), "-odecompile", str(dec)], check=True, capture_output=True)
        dec_line = next(i + 1 for i, line in enumerate(dec.read_text().splitlines()) if "* 3" in line)
        s = Session(args.newtc, tmp, ["-dbg"])
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(plain_pkg), "target": "tcp:%d" % s.port,
                                 "debugMap": str(dec.with_suffix(".nsdbg"))})
            check("a decompiled package: its debug map is found",
                  "3 of 3 functions found" in s.output_text("console"))
            s.request("setBreakpoints", {"source": {"path": str(dec)}, "breakpoints": [{"line": dec_line}]})
            s.request("configurationDone")
            while "Installed Debugee:TEST on the Newton." not in s.output_text("console"):
                s.event("output")
            s.client.send({"command": "evaluate", "arguments": {"expression": "DebugeeWork(5)", "context": "repl"}})
            stopped = s.event("stopped")
            r = s.request("stackTrace", {"threadId": 1})
            frames = r["body"]["stackFrames"] if r["success"] else []
            check("... a breakpoint in the decompiled source stops there; the stack shows that line "
                  "(the line table is on the desktop: |DAPAgent:Known|)",
                  stopped["body"]["reason"] == "breakpoint" and frames[:1]
                  and (os.path.realpath(frames[0].get("source", {}).get("path", "")), frames[0]["line"])
                  == (os.path.realpath(dec), dec_line))
            s.request("continue", {"threadId": 1})
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("decompiled package: no DAP errors (%s)" % e, False)
        logs = s.close()
        transcripts.append("\n".join(s.client.lines) + "\n--- logs ---\n" + logs)

        # an exception in a package's InstallScript, with All Exceptions: it stops there
        bad = Path(tmp) / "bad.ns"
        bad.write_text(BAD)
        bad_pkg = Path(tmp) / "bad.pkg"
        subprocess.run([args.newtc, "-g", "-script", str(bad), "-opkg", str(bad_pkg)], check=True, capture_output=True)
        s = Session(args.newtc, tmp, ["-dbg"])
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(bad_pkg), "target": "tcp:%d" % s.port})
            s.request("setExceptionBreakpoints", {"filters": ["all"]})
            s.request("configurationDone")
            stopped = s.event("stopped")
            r = s.request("stackTrace", {"threadId": 1})
            frames = r["body"]["stackFrames"] if r["success"] else []
            check("an exception in a package's InstallScript stops there (the install doesn't block)",
                  stopped["body"]["reason"] == "exception" and frames[:1]
                  and (frames[0].get("source", {}).get("path"), frames[0]["line"]) == (str(bad), 9))
            s.request("continue", {"threadId": 1})
            while "Installed BadInstall:TEST" not in s.output_text("console") \
                    and "couldn't install BadInstall:TEST" not in s.output_text("console"):
                s.event("output")
            check("... continue: the install ends", True)
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("InstallScript exception: no DAP errors (%s)" % e, False)
        logs = s.close()
        transcripts.append("\n".join(s.client.lines) + "\n--- logs ---\n" + logs)

        # attach (3.12): one Newton that stays on for four sessions
        port = free_port()
        env = dict(os.environ, NEWTC_NTK_DEVICE_SESSIONS="4")
        device = subprocess.Popen([args.newtc, "-dbg", "-ntk-device", "tcp-client:%d" % port],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
        # 1. launch: the package is installed
        s = Session(args.newtc, tmp, port=port, device=device)
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("launch", {"program": str(app_pkg), "target": "tcp:%d" % port})
            s.request("configurationDone")
            while "Installed Debugee:TEST on the Newton." not in s.output_text("console"):
                s.event("output")
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("attach, session 1: no DAP errors (%s)" % e, False)
        s.close()
        transcripts.append("\\n".join(s.client.lines))
        # 2. attach with the package: nothing installed, its breakpoints stop
        s = Session(args.newtc, tmp, port=port, device=device)
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            r = s.request("attach", {"program": str(app_pkg), "target": "tcp:%d" % port})
            check("attach with the package", r["success"])
            r = s.request("setBreakpoints", {"source": {"path": str(app)}, "breakpoints": [{"line": 14}]})
            s.request("configurationDone")
            while "Attached" not in s.output_text("console"):
                s.event("output")
            console = s.output_text("console")
            check("... attached to it, nothing installed, no warning",
                  "Attached to Debugee:TEST." in console and "Installed" not in console
                  and "Warning" not in console)
            s.client.send({"command": "evaluate", "arguments": {"expression": "DebugeeWork(5)", "context": "repl"}})
            stopped = s.event("stopped")
            r = s.request("stackTrace", {"threadId": 1})
            frames = r["body"]["stackFrames"] if r["success"] else []
            check("... a breakpoint in the installed package stops (line 14)",
                  stopped["body"]["reason"] == "breakpoint" and frames[:1] and frames[0]["line"] == 14)
            s.request("continue", {"threadId": 1})
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("attach, session 2: no DAP errors (%s)" % e, False)
        s.close()
        transcripts.append("\\n".join(s.client.lines))
        # 3. attach with another build of it: a warning
        s = Session(args.newtc, tmp, port=port, device=device)
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            s.request("attach", {"program": str(plain_pkg), "target": "tcp:%d" % port,
                                 "debugMap": str(dec.with_suffix(".nsdbg"))})
            s.request("configurationDone")
            while "Attached" not in s.output_text("console"):
                s.event("output")
            check("attach with another build: a warning",
                  "another build?" in s.output_text("console"))
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("attach, session 3: no DAP errors (%s)" % e, False)
        s.close()
        transcripts.append("\\n".join(s.client.lines))
        # 4. attach without a program
        s = Session(args.newtc, tmp, port=port, device=device)
        try:
            s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
            r = s.request("attach", {"program": str(app_pkg)})
            check("attach without a target: an error response", not r["success"] and "target" in r["message"])
            r = s.request("attach", {"target": "tcp:%d" % port})
            check("attach without a program", r["success"])
            s.request("configurationDone")
            while "Attached" not in s.output_text("console"):
                s.event("output")
            r = s.request("evaluate", {"expression": "DebugeeWork(5)", "context": "repl"})
            check("... the package still works; session 2's breakpoint is gone (its end removed it)",
                  r["success"] and r["body"]["result"] == "16")
            s.client.send({"command": "disconnect"})
        except dap_client.DAPError as e:
            check("attach, session 4: no DAP errors (%s)" % e, False)
        s.close()
        transcripts.append("\\n".join(s.client.lines))
        try:
            device.wait(timeout=15)
        except subprocess.TimeoutExpired:
            device.kill()
            device.wait()
        device_logs = device.stderr.read() + device.stdout.read()
        transcripts.append("--- device ---\\n" + device_logs)
        check("the device served four sessions, installed the package once, and ended",
              device.returncode == 0 and device_logs.count('installed "Debugee:TEST"') == 1
              and device_logs.count("Toolkit connected") == 4)

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
