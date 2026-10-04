#!/usr/bin/env python3
"""
A remote Newton for NewtonScript (plan step 3.5a, Matt/NTKRemote.{h,cc}): a
script run by `newtc -script` uses NTKOpen, NTKCall, NTKEvaluate, ... to
control `newtc -ntk-device` (the Newton), with a handler frame for what the
Newton sends:

  - NTKOpen, NTKWaitConnected, the handler's Connected and Object ('dante')
  - NTKCall: results as objects (also a function compiled with Compile())
  - NTKEvaluate: the Newton's output to the handler's Text
  - exceptions to the handler's Exception: eref (a frame), eerr (a code)
  - a break loop: BreakLoop(true), NTKCall while stopped, BreakLoop(nil)
  - LoadDataFile (NTK's), NTKInstallPackage, NTKDeletePackage with the
    Newton's answers (0, -10402)
  - an exception inside NTKCall's function: the Newton ends the connection,
    NTKCall throws, the handler's Disconnected
  - NTKCall without a connection throws

Usage: Test/ntk/test_remote.py [--newtc path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
from test_mnp import free_port  # noqa: E402

SCRIPT = r'''
NTKSetHandler({
  Connected: func(peer) Print("connected"),
  Disconnected: func(reason) Print("disconnected: " & reason),
  Text: func(text) Print("text: " & text),
  Exception: func(name, data) Print(["exception", name, data]),
  BreakLoop: func(entered) Print(["breakloop", entered]),
  Object: func(command, object) Print(["object", command, object]),
});
Print(NTKOpen("tcp:PORT"));
Print(["waited", NTKWaitConnected(10)]);
Print(["call", NTKCall(func() 6 * 7)]);
Print(["call", NTKCall(Compile("[1, \"two\", 'three]"))]);
NTKEvaluate(func() Print("hello"));
NTKCall(func() nil);
NTKEvaluate(func() NoSuchFunctionProbe());
NTKCall(func() nil);
NTKEvaluate(func() ExitBreakLoop());
NTKCall(func() nil);
NTKEvaluate(func() BreakLoop());
Print(["while stopped", NTKCall(func() 2 + 3)]);
NTKEvaluate(func() ExitBreakLoop());
NTKCall(func() nil);
pkg := LoadDataFile("PKG", 'package);
Print(["loaded", ClassOf(pkg), Length(pkg) > 1000]);
Print(["install", NTKInstallPackage(pkg)]);
Print(["installed", NTKCall(func() HasSlot(GetRoot(), '|hello:SIG|))]);
Print(["again", NTKInstallPackage(pkg)]);
Print(["delete", NTKDeletePackage("hello:SIG")]);
Print(["installed", NTKCall(func() HasSlot(GetRoot(), '|hello:SIG|))]);
try
  NTKCall(func() NoSuchFunctionProbe());
onexception |evt.ex| do
  Print(["call threw", CurrentException().message]);
Print(["connected", NTKIsConnected()]);
try
  NTKCall(func() 1);
onexception |evt.ex| do
  Print(["call threw", CurrentException().message]);
NTKClose();
Print("end");
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(REPO / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []

    def check(name, ok):
        checks.append((name, bool(ok)))

    with tempfile.TemporaryDirectory() as tmp:
        pkg = str(Path(tmp) / "hello.pkg")
        subprocess.run([args.newtc, "-hello", "-opkg", pkg], check=True, capture_output=True)
        port = free_port()
        script = Path(tmp) / "remote.ns"
        script.write_text(SCRIPT.replace("PORT", str(port)).replace("PKG", pkg))
        desk = subprocess.Popen([args.newtc, "-script", str(script)], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, text=True)
        time.sleep(0.5)
        dev = subprocess.Popen([args.newtc, "-ntk-device", "tcp-client:%d" % port],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            out, err = desk.communicate(timeout=60)
        except subprocess.TimeoutExpired:
            desk.kill()
            out, err = desk.communicate()
        try:
            dev_out, dev_err = dev.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            dev.kill()
            dev_out, dev_err = dev.communicate()

    lines = re.sub(r"\n +", "", out).splitlines()     # pretty-printed objects on one line
    check("open, connect: NTKOpen nil, Connected, NTKWaitConnected true",
          lines[:3] == ["nil", '"connected"', '["waited", true]'])
    check("the 'dante' object to Object", "[\"object\", \"fobj\", {interpretation: 'dante, data: {}}]" in lines)
    check("NTKCall: results as objects", '["call", 42]' in lines and "[\"call\", [1, \"two\", 'three]]" in lines)
    check("NTKEvaluate: the Newton's output to Text",
          '"text: \\"hello\\""' in lines and '"text: #2        nil"' in lines)
    # B33: newtc's interpreter names the function in 'value, the ROM in 'symbol
    check("an exception (eref) to Exception with its frame",
          any(l.startswith('["exception", "evt.ex.fr.intrp;type.ref.frame", {errorCode: -48808, ')
              and "'NoSuchFunctionProbe}]" in l for l in lines))
    check("an error (eerr) to Exception with its code", '["exception", "evt.ex.fr.intrp", -48800]' in lines)
    i = lines.index('["breakloop", true]') if '["breakloop", true]' in lines else -1
    check("a break loop: BreakLoop(true), NTKCall while stopped, BreakLoop(nil)",
          i >= 0 and '["while stopped", 5]' in lines[i:] and '["breakloop", nil]' in lines[i:])
    check("LoadDataFile: the package as a binary of class 'package", "[\"loaded\", 'package, true]" in lines)
    check("NTKInstallPackage: 0, installed", '["install", 0]' in lines
          and lines[lines.index('["install", 0]') + 1] == '["installed", true]')
    check("NTKInstallPackage again: -10402", '["again", -10402]' in lines)
    check("NTKDeletePackage: 0, removed", '["delete", 0]' in lines
          and lines[lines.index('["delete", 0]') + 1] == '["installed", nil]')
    check("an exception in NTKCall's function: the Newton ends the connection, NTKCall throws",
          any(l.startswith('"disconnected: ') for l in lines)
          and '["call threw", "The remote Newton ended the connection"]' in lines)
    check("... no longer connected", '["connected", nil]' in lines)
    check("NTKCall without a connection throws",
          '["call threw", "The remote Newton is not connected"]' in lines)
    check("the script ends", lines[-1:] == ['"end"'] and desk.returncode == 0)
    logs = err + dev_out + dev_err
    check("no sanitizer reports", "Sanitizer" not in logs and "runtime error" not in logs)

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n--- desktop ---\n" + out + "\n--- logs ---\n" + logs)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
