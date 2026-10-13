#!/usr/bin/env python3
"""
The "newtc" app for the Newton (plan step 3.14, Matt/Debugger/Connect.ns),
run in newtc itself (needs FLTK: its window, TestTap). The ROM's
connection is stubbed: ntpTetheredListener answers from a list (0: newtc
answered, -10021: no answer), NTKAlive says whether it is up.

  - `newtc -connect-package x.pkg` writes the package
  - opened as Extras does (ButtonToggle): no connection attempt (version 1
    had its own Toggle and connected instead, blocking the Newton)
  - Connect fails: not tried again (each attempt blocks the Newton);
    Connect again: connected (NTK's hello sent, "Connected to newtc.")
  - the connection drops: three more attempts, then it gives up and
    shows why
  - without "Stay connected" a connect, then Disconnect (the listener
    stopped)

Usage: Test/ntk/test_connect_app.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]

STUBS = '''// the ROM's connection, stubbed
DefGlobalVar('results, [-10021, 0, -10021, -10021, -10021]);   // see CHECK
DefGlobalVar('alive, nil);
DefGlobalFn('ntpTetheredListener, func(start, kind, machine)
begin
  if start then
  begin
    local r := results[0];
    ArrayRemoveCount(results, 0, 1);
    Print(["listen", kind, r, GetRoot().|Connect:newtc|.statusText]);
    if r = 0 then alive := true;
    r;
  end
  else
  begin
    alive := nil;
    Print("listener stopped");
    0;
  end
end);
DefGlobalFn('NTKAlive, func() alive);
DefGlobalFn('NTKSend, func(obj) Print(["sent", obj.interpretation]));
ref0;
'''

CHECK = '''// the app is open (-run): close it and open it as Extras does (the
// ROM's ButtonToggle, which sends Toggle() to the base view); then tap its
// button
DefGlobalVar('app, GetRoot().|Connect:newtc|);
app.retryDelay := 300;
app.watchInterval := 200;
app:Close();
app:ButtonToggle();
Print(["open", app.viewCObject <> nil, app.connState, app.statusText, app.buttonView.text]);
TestTap(app.buttonView, nil);
AddDelayedCall(func()
  begin
    Print(["after a failed connect", app.connState, app.statusText, app.buttonView.text]);
    TestTap(app.buttonView, nil);
  end, [], 500);
AddDelayedCall(func()
  begin
    Print(["after connect", app.connState, app.statusText, app.buttonView.text]);
    alive := nil;
  end, [], 1500);
AddDelayedCall(func()
  begin
    Print(["after the drop", app.connState, app.statusText, app.buttonView.text]);
    app.stayConnected := nil;
    results := [0];
    TestTap(app.buttonView, nil);
  end, [], 4500);
AddDelayedCall(func()
  begin
    Print(["connected again", app.connState]);
    TestTap(app.buttonView, nil);
  end, [], 5500);
AddDelayedCall(func()
  begin
    Print(["disconnected", app.connState, app.statusText, app.buttonView.text]);
    app:Close();
  end, [], 6000);
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    has_fltk = subprocess.run([args.newtc, "-s", "Print(HasSlot(functions, 'TestWindow))"],
                              capture_output=True, timeout=30, stdin=subprocess.DEVNULL)
    if b"true" not in has_fltk.stdout:
        print("skip    newtc without FLTK")
        return 0

    with tempfile.TemporaryDirectory() as tmp:
        pkg = Path(tmp) / "newtc.pkg"
        stubs = Path(tmp) / "stubs.ns"
        stubs.write_text(STUBS)
        taps = Path(tmp) / "taps.ns"
        taps.write_text(CHECK)
        proc = subprocess.run([args.newtc, "-connect-package", str(pkg), "-pkg", str(pkg),
                               "-script", str(stubs), "-run", "-script", str(taps)],
                              capture_output=True, timeout=60, stdin=subprocess.DEVNULL, cwd=tmp)
        out = proc.stdout.decode("utf-8", "replace")
        err = proc.stderr.decode("utf-8", "replace")
        check("-connect-package writes the package", pkg.exists() and pkg.stat().st_size > 1000)

    lines = [line for line in out.splitlines() if line.startswith("[") or line.startswith('"')]
    text = "\n".join(lines)
    check("ButtonToggle (Extras) opens the app, no connection attempt (version 1 connected)",
          '["open", true, \'idle, "Not connected.", "Connect"]' in text
          and text.index('["open"') < text.index('"listen"'))
    check("Connect: MNP on the serial port (3), status Connecting",
          '["listen", 3, -10021, "Connecting to newtc..."]' in text)
    check("a Connect that fails isn't tried again (each attempt blocks the Newton)",
          '["after a failed connect", \'idle, "No answer from newtc.' in text
          and text.split('["after a failed connect"', 1)[-1].split("\n", 1)[0].endswith('"Connect"]'))
    check("connected: NTK's hello, status, button Disconnect",
          '["sent", \'dante]' in text
          and '["after connect", \'connected, "Connected to newtc.", "Disconnect"]' in text)
    check("the connection drops: three attempts",
          '"Connecting to newtc (again, 3 of 3)..."' in text and text.count('"listen"') >= 5)
    check("... then it gives up and says why",
          '["after the drop", \'idle, "No answer from newtc.' in text
          and text.split('["after the drop"', 1)[-1].split("\n", 1)[0].endswith('"Connect"]'))
    check("without Stay connected: connects",
          '["connected again", \'connected]' in text)
    check("Disconnect stops the listener",
          '"listener stopped"' in text
          and '["disconnected", \'idle, "Not connected.", "Connect"]' in text)
    check("no exceptions", "Exception" not in out + err)

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("--- stdout ---\n" + out[-4000:] + "\n--- stderr ---\n" + err[-3000:])
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
