#!/usr/bin/env python3
"""
A small Debug Adapter Protocol client for testing `newtc -dap`.

As a module: run_script(newtc, script_text, cwd, substitutions) runs newtc,
plays a .dap script against it, and returns the transcript.

A .dap script has one command per line:

  {"command": "initialize", "arguments": {...}}
      send a request (the client adds "seq" and "type") and read messages
      until its response arrives
  wait <event>
      read messages until that event arrives (e.g. `wait terminated`)
  eof
      close newtc's stdin (the client went away)
  mask /regex/replacement/
      replace in the whole transcript (for values that change from run to
      run, like where a pause stops); Python re syntax
  # comment, or an empty line

The transcript has one line per message: `-> {...}` for sent, `<- {...}`
for received, as compact JSON in the order of arrival. At the end, newtc
must exit on its own (after "disconnect" or "eof"); anything left on
stdout is read first.

As a program: dap_client.py [--newtc path] <script.dap> [key=value ...]
prints the transcript (for trying things out by hand).
"""

import json
import re
import select
import subprocess
import sys
import time
from pathlib import Path

TIMEOUT = 10  # seconds per wait


class DAPError(Exception):
    pass


class Client:
    def __init__(self, proc):
        self.proc = proc
        self.seq = 0
        self.buffer = b""
        self.lines = []

    def send(self, message):
        self.seq += 1
        message = {"seq": self.seq, "type": "request", **message}
        body = json.dumps(message, separators=(",", ":")).encode()
        self.proc.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        self.proc.stdin.flush()
        self.lines.append("-> " + body.decode())
        return self.seq

    def _read_some(self, deadline):
        fd = self.proc.stdout.fileno()
        while time.time() < deadline:
            ready, _, _ = select.select([fd], [], [], 0.1)
            if ready:
                data = self.proc.stdout.read1(65536)
                if not data:
                    return False
                self.buffer += data
                return True
        raise DAPError(f"timeout after {TIMEOUT}s")

    def receive(self):
        """The next message, or None at end of output."""
        deadline = time.time() + TIMEOUT
        while True:
            header_end = self.buffer.find(b"\r\n\r\n")
            if header_end >= 0:
                length = None
                for line in self.buffer[:header_end].split(b"\r\n"):
                    name, _, value = line.partition(b":")
                    if name.strip().lower() == b"content-length":
                        length = int(value)
                if length is None:
                    raise DAPError("header without Content-Length: %r" % self.buffer[:header_end])
                start = header_end + 4
                if len(self.buffer) >= start + length:
                    body = self.buffer[start:start + length]
                    self.buffer = self.buffer[start + length:]
                    self.lines.append("<- " + body.decode())
                    return json.loads(body)
            if not self._read_some(deadline):
                if self.buffer:
                    self.lines.append("<- (incomplete) " + self.buffer.decode("utf-8", "replace"))
                    self.buffer = b""
                return None

    def request(self, message):
        seq = self.send(message)
        while True:
            m = self.receive()
            if m is None:
                raise DAPError(f"no response to request {seq}")
            if m.get("type") == "response" and m.get("request_seq") == seq:
                return m

    def wait_event(self, name):
        while True:
            m = self.receive()
            if m is None:
                raise DAPError(f"end of output while waiting for event {name}")
            if m.get("type") == "event" and m.get("event") == name:
                return m


def run_script(newtc, script, cwd, substitutions, extra_args=()):
    """Play `script` against `newtc -dap`. Returns (transcript, stderr, exit code)."""
    proc = subprocess.Popen([str(newtc), *extra_args, "-dap"], cwd=cwd,
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    client = Client(proc)
    masks = []
    try:
        for line in script.splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            for key, value in substitutions.items():
                line = line.replace("$" + key, value)
            if line.startswith("mask "):
                pattern, replacement = line[6:-1].split("/", 1)
                masks.append((re.compile(pattern), replacement))
            elif line.startswith("wait "):
                client.wait_event(line[5:].strip())
            elif line == "eof":
                proc.stdin.close()
            else:
                client.request(json.loads(line))
        while client.receive() is not None:
            pass
        proc.wait(timeout=TIMEOUT)
    except (DAPError, subprocess.TimeoutExpired) as e:
        client.lines.append(f"--- ERROR: {e} ---")
        proc.kill()
        proc.wait()
    stderr = proc.stderr.read().decode("utf-8", "replace")
    transcript = "\n".join(client.lines) + "\n"
    for pattern, replacement in masks:
        transcript = pattern.sub(replacement, transcript)
    return transcript, stderr, proc.returncode


def main():
    import argparse
    repo = Path(__file__).resolve().parents[2]
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(repo / "build" / "VSCode" / "newtc"))
    ap.add_argument("script")
    ap.add_argument("subst", nargs="*", help="key=value, replaces $key in the script")
    args = ap.parse_args()
    subst = dict(s.split("=", 1) for s in args.subst)
    script = Path(args.script)
    transcript, stderr, code = run_script(args.newtc, script.read_text(), script.parent, subst)
    print(transcript + "--- stderr ---\n" + stderr + f"--- exit code {code} ---")


if __name__ == "__main__":
    main()
