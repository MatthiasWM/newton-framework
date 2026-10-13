#!/usr/bin/env python3
"""
Upload progress and timing (plan step 3.16). The Newton is `newtc -dbg
-ntk-device` at a serial line's speed (NEWTC_NTK_DEVICE_BPS=38400), so
uploads take seconds:

  - the agent's upload (a fresh device) and the package's show their
    progress in the Debug Console on one line: "Uploading <name> (<n> KB)
    .......... done in <t> s", the dots arriving while it goes
  - NEWTC_DAP_TIMES=1: what the session's start did when ("[1.2 s] agent
    checked ..."); without it, none of that

Usage: Test/ntk/test_dap_upload.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import json
import os
import re
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

UPLOAD = re.compile(r"Uploading (\S+) \((\d+) KB\) \.{10} done in \d+\.\d s\n")


def console_events(s):
    """The texts of the "console" output events, in order."""
    texts = []
    for line in s.client.lines:
        if line.startswith("<- {"):
            m = json.loads(line[3:])
            if m.get("event") == "output" and m["body"]["category"] == "console":
                texts.append(m["body"]["output"])
    return texts


def session(newtc, tmp, pkg, times):
    """One launch of pkg on a fresh device at 38400 bps; its console events."""
    os.environ["NEWTC_NTK_DEVICE_BPS"] = "38400"
    os.environ["NEWTC_DAP_TIMES"] = "1" if times else "0"
    s = Session(newtc, tmp, ["-dbg"])
    try:
        s.request("initialize", {"clientID": "test", "adapterID": "newtonscript"})
        s.request("launch", {"program": str(pkg), "target": "tcp:%d" % s.port})
        s.request("configurationDone")
        while "Installed hello:SIG on the Newton." not in s.output_text("console"):
            s.event("output")
        s.client.send({"command": "disconnect"})
    except dap_client.DAPError as e:
        print("DAP error: %s" % e)
    logs = s.close()
    return console_events(s), "\n".join(s.client.lines) + "\n--- logs ---\n" + logs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []
    transcripts = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    with tempfile.TemporaryDirectory() as tmp:
        pkg = Path(tmp) / "hello.pkg"
        subprocess.run([args.newtc, "-hello", "-opkg", str(pkg)], check=True, capture_output=True,
                       stdin=subprocess.DEVNULL)

        events, transcript = session(args.newtc, tmp, pkg, True)
        transcripts.append(transcript)
        text = "".join(events)
        uploads = UPLOAD.findall(text)
        check("the agent's upload: one line with its progress",
              any(name == "DAPAgent:newtc" and int(kb) > 1 for name, kb in uploads))
        check("the package's upload: one line with its progress",
              any(name == "hello.pkg" for name, kb in uploads))
        # the agent's dots: several events (they come while it uploads)
        start = next((i for i, t in enumerate(events) if t.startswith("Uploading DAPAgent:newtc")), None)
        dot_events = 0
        if start is not None:
            for t in events[start + 1:]:
                if t.strip(".") == "":
                    dot_events += 1
                else:
                    break
        check("... the dots arrive while it goes (%d events)" % dot_events, dot_events >= 3)
        check("NEWTC_DAP_TIMES=1: the steps with their times",
              re.search(r"\[\d+\.\d s\] connected\n", text)
              and re.search(r"\[\d+\.\d s\] agent checked", text)
              and re.search(r"\[\d+\.\d s\] installed \(the InstallScript ran\)", text))

        events, transcript = session(args.newtc, tmp, pkg, False)
        transcripts.append(transcript)
        text = "".join(events)
        check("without NEWTC_DAP_TIMES: no times, the progress still",
              not re.search(r"\[\d+\.\d s\]", text) and len(UPLOAD.findall(text)) == 2)

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n\n".join(t[-5000:] for t in transcripts))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
