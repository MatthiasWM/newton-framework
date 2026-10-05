#!/usr/bin/env python3
"""
Getting packages onto a remote Newton (plan step 3.5b,
Matt/Debugger/Remote.ns, Agent.ns, and NTKMakePackage, NTKPackageName,
NTKLibrary in Matt/NTKRemote.cc), against `newtc -ntk-device`:

  - the agent: missing, EnsureAgent installs it (version 1), a second
    EnsureAgent leaves it (installed once)
  - InstallPackage replaces a package of the same name (a rebuild): 'pkgX',
    then 'pkg ', both times 0
  - NS Debug Tools: missing on a stock device; EnsureDebugTools uploads the
    package set in debugToolsPackage (here a stand-in that defines
    NSDOriginalBreakLoop, since Apple's has ARM code); an unreadable
    package gives a message; with -dbg (NSDT there) nothing is uploaded

Usage: Test/ntk/test_deploy.py [--newtc path]
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

# A stand-in for NS Debug Tools: an auto part that defines the function
# HasDebugTools looks for, and removes it again.
TOOLS = r'''
{
  signature: 'package0, id: "xxxx", flags: {noCompression: true}, version: 1,
  copyright: "test", name: "NS Debug Tools:PIE", modifyDate: 0, info: "test",
  part: [{
    offset: 0, size: 0, type: "auto", flags: {type: 'nos, Notify: true}, info: "test",
    data: {
      InstallScript: func(partFrame)
      begin
        DefGlobalFn(EnsureInternal('NSDOriginalBreakLoop), func() nil);
        return EnsureInternal({removeScript: func(r) RemoveSlot(functions, 'NSDOriginalBreakLoop)});
      end
    }
  }]
}
'''

SCRIPT = r'''
lib := NTKLibrary();
NTKOpen("tcp:PORT");
Print(["connected", NTKWaitConnected(10)]);
Print(["agent before", lib:InstalledAgentVersion()]);
Print(["ensure agent", lib:EnsureAgent()]);
Print(["agent after", lib:InstalledAgentVersion()]);
Print(["ensure agent again", lib:EnsureAgent()]);
pkg := LoadDataFile("PKG", 'package);
Print(["install", lib:InstallPackage(pkg)]);
Print(["install again", lib:InstallPackage(pkg)]);
Print(["installed", NTKCall(func() HasSlot(GetRoot(), '|hello:SIG|))]);
Print(["tools before", lib:HasDebugTools()]);
lib.debugToolsPackage := "/nonexistent/NS Debug Tools.pkg";
Print(["ensure tools, unreadable", lib:EnsureDebugTools()]);
lib.debugToolsPackage := "TOOLS";
Print(["ensure tools", lib:EnsureDebugTools()]);
Print(["tools after", lib:HasDebugTools()]);
NTKClose();
Print("end");
'''


def run(newtc, script, dev_args=()):
    port = free_port()
    script_path = Path(script[1]) / "deploy.ns"
    script_path.write_text(script[0].replace("PORT", str(port)))
    desk = subprocess.Popen([newtc, "-script", str(script_path)], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    time.sleep(0.5)
    dev = subprocess.Popen([newtc] + list(dev_args) + ["-ntk-device", "tcp-client:%d" % port],
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
    return re.sub(r"\n +", "", out).splitlines(), err + dev_out + dev_err, desk.returncode


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
        tools_ns = Path(tmp) / "tools.ns"
        tools_ns.write_text(TOOLS)
        tools = str(Path(tmp) / "tools.pkg")
        subprocess.run([args.newtc, "-script", str(tools_ns), "-opkg", tools], check=True, capture_output=True)
        script = SCRIPT.replace("PKG", pkg).replace("TOOLS", tools)

        lines, logs, code = run(args.newtc, (script, tmp))
        check("connected", '["connected", true]' in lines)
        check("the agent: not there, then installed (version 11)",
              '["agent before", nil]' in lines and '["ensure agent", 0]' in lines
              and '["agent after", 11]' in lines)
        check("EnsureAgent again: there already, not installed again",
              '["ensure agent again", 0]' in lines and logs.count('installed "DAPAgent:newtc"') == 1)
        check("InstallPackage twice: replaced (deleted, installed), 0 both times",
              '["install", 0]' in lines and '["install again", 0]' in lines
              and '["installed", true]' in lines and logs.count('removed "hello:SIG"') == 1
              and logs.count('installed "hello:SIG"') == 2)
        check("NS Debug Tools: missing on a stock device", '["tools before", nil]' in lines)
        check("EnsureDebugTools: an unreadable package gives a message",
              '["ensure tools, unreadable", "Can\'t read NS Debug Tools: /nonexistent/NS Debug Tools.pkg"]'
              in lines)
        check("EnsureDebugTools: uploads the package, then they are there",
              '["ensure tools", 0]' in lines and '["tools after", true]' in lines
              and 'installed "NS Debug Tools:PIE"' in logs)
        check("the script ends", lines[-1:] == ['"end"'] and code == 0)
        check("no sanitizer reports", "Sanitizer" not in logs and "runtime error" not in logs)

        # with NS Debug Tools on the device (-dbg): nothing to upload
        lines2, logs2, code2 = run(args.newtc, (script, tmp), ["-dbg"])
        check("with -dbg: NS Debug Tools there, nothing uploaded",
              '["tools before", true]' in lines2 and '["ensure tools", 0]' in lines2
              and 'installed "NS Debug Tools:PIE"' not in logs2)

    failed = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
    if failed:
        print("\n--- desktop ---\n" + "\n".join(lines) + "\n--- logs ---\n" + logs)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
