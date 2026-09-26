#!/usr/bin/env python3
"""
Debug maps of decompiled packages (plan step 9.1).

`newtc -pkg P -odecompile X.ns` writes the decompiled source and X.nsdbg:
for each function its path, a hash of its instructions, and the source
lines of its statements (pc, line). For each package this checks:

  found   loading the map (`-pkg P -nsdbg X.nsdbg`) finds its functions
          again (by the hash of their instructions);
  lines   the map's lines agree with an independent source of truth: X.ns
          compiled with -g (our compiler writes its own line tables). The
          bytecode differs (NTK's compiler vs ours), but the statements are
          the same, so every line the map names should be a statement line
          of the recompiled function at the same path.

Usage:
    python3 Test/nsdbg_check.py --batch Test/corpus_results/latest_manifest.json --limit 50
    python3 Test/nsdbg_check.py <pkg> ...

Run from the repo root. Prints per package: found/total functions, and
lines: the map's lines that are statement lines in the recompiled code.
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
NEWTC = REPO / "build" / "VSCode" / "newtc"
TIMEOUT = 120

# prints, for each path of the map, the lines of the recompiled function's
# line table (ref0: the recompiled package)
LINES_OF = r"""
global LinesAt(root, path)
begin
  local obj := root;
  foreach step in path do
  begin
    local next := nil;
    if IsInteger(step) and IsArray(obj) and step < Length(obj) then
      next := obj[step];
    if IsString(step) and IsFrame(obj) then
      next := GetSlot(obj, Intern(step));
    obj := next;
  end;
  if IsFrame(obj) and HasSlot(obj, 'bcFunc) then obj := obj.bcFunc;
  if not (IsFrame(obj) and HasSlot(obj, 'lineTable)) then return [];
  local lines := [];
  for i := 2 to Length(obj.lineTable) - 1 by 2 do
    AddArraySlot(lines, obj.lineTable[i]);
  lines;
end;
"""


def run(args, cwd):
    return subprocess.run([str(NEWTC), *args], cwd=cwd, capture_output=True, timeout=TIMEOUT)


def check(pkg):
    try:
        return check_package(pkg)
    except Exception as e:     # report it, don't stop the batch
        return pkg, None, None, None, f"ERROR {type(e).__name__}: {e}"


def check_package(pkg):
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        made = run(["-pkg", str(pkg), "-odecompile", "x.ns"], tmp)
        if made.returncode != 0 or not (tmp / "x.nsdbg").exists():
            return pkg, None, None, None, "decompile failed"
        try:
            dbg = json.loads((tmp / "x.nsdbg").read_bytes())
        except ValueError as e:
            (Path("/tmp") / (Path(pkg).stem + ".bad.nsdbg")).write_bytes((tmp / "x.nsdbg").read_bytes())
            return pkg, None, None, None, f"MAP NOT JSON ({e}): /tmp/{Path(pkg).stem}.bad.nsdbg"
        functions = dbg["functions"]
        loaded = run(["-pkg", str(pkg), "-nsdbg", "x.nsdbg"], tmp)
        m = re.search(rb"(\d+) of (\d+) functions found", loaded.stderr)
        found = int(m.group(1)) if m else 0

        # the recompiled lines, per function
        lines = [LINES_OF]
        for f in functions:
            lines.append("Print(JSONStringify(LinesAt(ref0, JSONParse(%s))));"
                         % json.dumps(json.dumps(f["path"])))
        (tmp / "lines.ns").write_text("\n".join(lines) + "\n")
        again = run(["-g", "-script", "x.ns", "-script", "lines.ns"], tmp)
        printed = [line for line in again.stdout.decode("utf-8", "replace").splitlines()
                   if line.startswith('"[')]
        if len(printed) != len(functions):
            return pkg, found, len(functions), None, "recompile failed"
        agree = total = 0
        for f, text in zip(functions, printed):
            recompiled = set(json.loads(json.loads(text)))
            if not recompiled:
                continue
            mapped = f["lines"][1::2]
            total += len(mapped)
            agree += sum(1 for line in mapped if line in recompiled)
        return pkg, found, len(functions), (agree, total), ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pkgs", nargs="*")
    ap.add_argument("--batch")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--jobs", type=int, default=8)
    args = ap.parse_args()
    pkgs = list(args.pkgs)
    if args.batch:
        manifest = json.loads(Path(args.batch).read_text())
        pkgs += [e["pkg"] for e in manifest["packages"] if e.get("status") == "CLEAN"]
    if args.limit:
        pkgs = pkgs[:args.limit]
    sums = [0, 0, 0, 0]
    with ThreadPoolExecutor(args.jobs) as pool:
        for pkg, found, total, lines, note in pool.map(check, pkgs):
            if found is None:
                print(f"skip    {Path(pkg).name}: {note}")
                continue
            text = f"found {found}/{total}"
            if lines:
                text += f", lines {lines[0]}/{lines[1]}"
                sums[2] += lines[0]
                sums[3] += lines[1]
            print(f"{'ok' if found == total else 'PART':7} {Path(pkg).name}: {text} {note}")
            sums[0] += found
            sums[1] += total
    print(f"\nfunctions found {sums[0]}/{sums[1]}; map lines that are statement lines "
          f"of the recompiled code {sums[2]}/{sums[3]}")


if __name__ == "__main__":
    sys.exit(main())
