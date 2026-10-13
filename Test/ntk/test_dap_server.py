#!/usr/bin/env python3
"""
The background server (plan step 3.13): `newtc -dap-server <port>` serves
one DAP session after the other, and a remote Newton's link (here `newtc
-dbg -ntk-device`) stays open from session to session:

  - session 1 launches a .ns program on the Newton (it connects once)
  - between sessions the link stays up (the idle ping, NEWTC_NTK_PING=1);
    a break loop on the Newton is left at once (nobody would see it), and
    what the Newton prints goes to the server's terminal
  - session 2 attaches: "still connected" (no new connection), the
    program's global from session 1 is still there; it evaluates
    something that calls BreakLoop() after a second, and disconnects
    before that
  - session 3 launches again over the same link
  - a client that disconnects before launching ends only its session
  - the server is still running at the end

Usage: Test/ntk/test_dap_server.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(REPO / "Test" / "dbg"))
import dap_client  # noqa: E402
from test_mnp import free_port  # noqa: E402

PROGRAM = '''Print("session one");
serverProbe := 41;
'''

PROGRAM3 = '''Print("session three: " & serverProbe);
'''


def connect(port):
    """A DAP client on the server's port (it may still be starting)."""
    for _ in range(200):
        try:
            return dap_client.Client(dap_client.SocketConnection(
                socket.create_connection(("127.0.0.1", port))))
        except OSError:
            time.sleep(0.05)
    raise dap_client.DAPError("the server doesn't listen on %d" % port)


def console(client):
    """The "console" output of a session so far."""
    import json
    text = ""
    for line in client.lines:
        if line.startswith("<- {"):
            m = json.loads(line[3:])
            if m.get("event") == "output" and m["body"]["category"] in ("console", "stdout"):
                text += m["body"]["output"]
    return text


def end(client):
    """disconnect, and the server closes the connection."""
    r = client.request({"command": "disconnect"})
    client.connection.sock.close()
    return r["success"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    with tempfile.TemporaryDirectory() as tmp:
        program = Path(tmp) / "one.ns"
        program.write_text(PROGRAM)
        program3 = Path(tmp) / "three.ns"
        program3.write_text(PROGRAM3)
        dap_port, ntk_port = free_port(), free_port()
        target = "tcp:%d" % ntk_port
        env = dict(os.environ, NEWTC_NTK_PING="1")
        log = ["-dap-log", os.environ["DAP_LOG"]] if os.environ.get("DAP_LOG") else []
        server = subprocess.Popen([args.newtc] + log + ["-dap-server", str(dap_port)], cwd=tmp, env=env,
                                  stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        device = subprocess.Popen([args.newtc, "-dbg", "-ntk-device", "tcp-client:%d" % ntk_port],
                                  stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            # a client that leaves before launching
            c = connect(dap_port)
            c.request({"command": "initialize", "arguments": {"adapterID": "newtonscript"}})
            end(c)

            # 1: launch, the Newton connects
            c = connect(dap_port)
            c.request({"command": "initialize", "arguments": {"adapterID": "newtonscript"}})
            r = c.request({"command": "launch", "arguments": {"program": str(program), "target": target}})
            check("a client that left before launching: the next one is served", r["success"])
            c.request({"command": "configurationDone"})
            c.wait_event("terminated")
            text = console(c)
            check("session 1: the Newton connects, the program runs",
                  "The Newton is connected." in text and "session one" in text)
            check("session 1 ends with disconnect", end(c))

            time.sleep(3)       # the idle pings (every second) keep the link up

            # 2: attach over the same link
            c = connect(dap_port)
            c.request({"command": "initialize", "arguments": {"adapterID": "newtonscript"}})
            c.request({"command": "attach", "arguments": {"target": target}})
            c.request({"command": "configurationDone"})
            r = c.request({"command": "evaluate", "arguments": {"expression": "serverProbe + 1", "context": "repl"}})
            text = console(c)
            check("session 2: still connected, no new connection",
                  "still connected" in text and "Waiting for a Newton" not in text)
            check("session 2: the global from session 1 is there",
                  r["success"] and r["body"]["result"] == "42")
            # ~0.6 s of work (-dbg), then BreakLoop(): it comes after the disconnect
            c.send({"command": "evaluate", "arguments": {"expression":
                "begin for i := 1 to 2000000 do nil; BreakLoop(); Print(\"went on\") end",
                "context": "repl"}})
            check("session 2 ends with disconnect", end(c))

            time.sleep(3)       # the BreakLoop() comes while no session runs

            # 3: launch again
            c = connect(dap_port)
            c.request({"command": "initialize", "arguments": {"adapterID": "newtonscript"}})
            c.request({"command": "launch", "arguments": {"program": str(program3), "target": target}})
            c.request({"command": "configurationDone"})
            c.wait_event("terminated")
            text = console(c)
            check("session 3: still connected, the program runs",
                  "still connected" in text and "session three: 41" in text)
            check("session 3 ends with disconnect", end(c))
            time.sleep(0.5)
            check("the server still runs", server.poll() is None)
        except dap_client.DAPError as e:
            check("no DAP error (%s)" % e, False)
        finally:
            for proc in (server, device):
                proc.kill()
            out, err = server.communicate()
            out, err = out.decode("utf-8", "replace"), err.decode("utf-8", "replace")
            dout, derr = device.communicate()
            if os.environ.get("SHOW_DEVICE"):
                print("--- device ---\n" + dout.decode("utf-8", "replace")[-3000:] + derr.decode("utf-8", "replace")[-3000:])

        check("between sessions: the break loop is left at once",
              "no debug session: it goes on" in err and "went on" in out)
        check("the server says when a client comes and goes",
              err.count("DAP client connected") == 4 and err.count("waiting for the next one") == 4)

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("--- server stdout ---\n" + out[-3000:] + "\n--- server stderr ---\n" + err[-3000:])
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
