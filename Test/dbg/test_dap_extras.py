#!/usr/bin/env python3
"""
newtc's DAP options for working on newtc itself (plan step 5.6):

  -dap-log <file>     every message, both ways, in the transcript format of
                      the .dap test cases ("-> " to newtc, "<- " from it)
  -dap-server <port>  one DAP session over TCP (localhost); stdin/stdout
                      stay free, so newtc can run in a debugger

The .dap cases (run_dbg_tests.py) can't test these: they need a second
file, or a socket instead of the pipes. This test plays
cases/dap_breakpoint.dap both ways and checks that the log equals the
client's transcript, and that the session over TCP gives the same
transcript as over stdin/stdout.

Usage: Test/dbg/test_dap_extras.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
import dap_client  # noqa: E402

CASE = HERE / "cases" / "dap_breakpoint.ns"


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    script = CASE.with_suffix(".dap").read_text()
    subst = {"PROGRAM": str(CASE), "DIR": str(CASE.parent)}
    checks = []

    # -dap-log: the log equals what the client saw
    with tempfile.TemporaryDirectory() as tmp:
        log = Path(tmp) / "dap.log"
        piped, _, code = dap_client.run_script(args.newtc, script, CASE.parent, subst,
                                               ["-dap-log", str(log)])
        logged = log.read_text() if log.exists() else ""
        checks.append(("session over stdin/stdout ends normally", code == 0 and "terminated" in piped))
        checks.append(("-dap-log equals the client's transcript", logged == piped))

    # -dap-server: the same session over TCP
    port = free_port()
    proc = subprocess.Popen([args.newtc, "-dap-server", str(port)], cwd=CASE.parent,
                            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    sock = None
    for _ in range(100):            # until newtc listens
        try:
            sock = socket.create_connection(("127.0.0.1", port))
            break
        except OSError:
            time.sleep(0.05)
    served = ""
    if sock:
        client = dap_client.Client(dap_client.SocketConnection(sock))
        try:
            masks = dap_client.play(client, script, subst)
            served = dap_client.transcript_of(client, masks)
        except dap_client.DAPError as e:
            served = dap_client.transcript_of(client) + f"--- ERROR: {e} ---\n"
        sock.close()
    try:
        out, err = proc.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
    out = out.decode("utf-8", "replace")
    err = err.decode("utf-8", "replace")
    checks.append(("-dap-server accepts a client", sock is not None))
    checks.append(("-dap-server says where it listens", f"waiting for a DAP client on port {port}" in err))
    checks.append(("same session over TCP as over stdin/stdout", served == piped))
    checks.append(("stdout stays free for other output", "BREAKPOINTS ENABLED" in out))
    checks.append(("-dap-server exits after the session", proc.returncode == 0))

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n--- over stdin/stdout ---\n" + piped + "\n--- over TCP ---\n" + served
              + "\n--- newtc stdout ---\n" + out + "\n--- newtc stderr ---\n" + err)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
