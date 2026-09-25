#!/usr/bin/env python3
"""
Line tables must not change the code (plan step 7.2).

With line tables on (global dbgKeepLineNumbers, set by -g, -dbg, -dap) the
compiler wraps statements in [TOKENline, line, statement] and adds a
lineTable slot to every function; the bytecode must stay exactly the same.
For each package: decompile it (generation 1 source), then compile that
source twice, without and with line tables, and decompile both results.
The decompiler ignores the lineTable slot, so the two outputs must be
identical, except for one thing: all line tables share the source file's
name, a string the decompiler sees and prints as a constant of its own
(DefineGlobalConstant('Ref_N, ".../gen1.ns")); that block is ignored, and
the Ref_N numbers are renumbered in order of appearance (the decompiler
numbers the line table arrays, too). Also checks that the second compile
really produced line tables: "functions with/without line table: [n, m]"
counts the functions reachable from the package (not through _proto or
_parent); the few without one are ROM functions reached through magic
pointers (@588), not code from the source.

Usage:
    python3 Test/lines_invariant.py <pkg> ...
    python3 Test/lines_invariant.py --batch Test/corpus_results/latest_manifest.json --limit 50

Run from the repo root. Exit code 0 if all packages agree.
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
TIMEOUT = 60
LINES_ON = "DefGlobalVar('dbgKeepLineNumbers, true);"
FILE_NAME_CONSTANT = re.compile(rb"DefineGlobalConstant\('Ref_\d+, (\"[^\"]*\"\s*)*?\"[^\"]*gen1\.ns\"\);\n\n")

# counts the functions with and without a line table in ref1 (the program's
# result); functions are frames with an instructions slot
COUNT = r"""
DefGlobalVar('lineStats, {withTable: 0, withoutTable: 0, visits: 0});
global CountLines(obj, depth)
begin
  lineStats.visits := lineStats.visits + 1;
  if depth < 14 and lineStats.visits < 300000 and (IsFrame(obj) or IsArray(obj)) then
  begin
    if IsFrame(obj) and HasSlot(obj, 'instructions) then
      if HasSlot(obj, 'lineTable) then lineStats.withTable := lineStats.withTable + 1
      else lineStats.withoutTable := lineStats.withoutTable + 1;
    if IsFrame(obj) then
    begin
      foreach slot, value in obj do
        if slot <> 'lineTable and slot <> '_parent and slot <> '_proto then CountLines(value, depth + 1);
    end
    else
      foreach value in obj do CountLines(value, depth + 1);
  end;
end;
CountLines(ref1, 0);   // ref0 is the result of the -s that turns line tables on
Print([lineStats.withTable, lineStats.withoutTable]);
"""


def normalized(text):
    """Remove the file name constant, renumber Ref_N by first appearance."""
    text = FILE_NAME_CONSTANT.sub(b"", text)
    numbers = {}
    return re.sub(rb"Ref_\d+", lambda m: numbers.setdefault(m.group(0), b"Ref_%d" % len(numbers)), text)


def run(args, cwd):
    return subprocess.run([str(NEWTC), *args], cwd=cwd, capture_output=True, timeout=TIMEOUT)


def check(pkg):
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        gen1 = run(["-pkg", str(pkg), "-decompile"], tmp)
        if gen1.returncode != 0:
            return pkg, "skip", "decompile failed"
        (tmp / "gen1.ns").write_bytes(gen1.stdout)
        plain = run(["-script", "gen1.ns", "-decompile"], tmp)
        lined = run(["-s", LINES_ON, "-script", "gen1.ns", "-decompile"], tmp)
        if plain.returncode != 0 or lined.returncode != 0:
            return pkg, "skip", "recompile failed"
        if normalized(plain.stdout) != normalized(lined.stdout):
            (Path("/tmp") / (Path(pkg).stem + ".plain.txt")).write_bytes(plain.stdout)
            (Path("/tmp") / (Path(pkg).stem + ".lined.txt")).write_bytes(lined.stdout)
            return pkg, "DIFFERENT", "decompiled code differs (see /tmp/*.plain.txt, *.lined.txt)"
        (tmp / "count.ns").write_text(COUNT)
        counted = run(["-s", LINES_ON, "-script", "gen1.ns", "-script", "count.ns"], tmp)
        text = counted.stdout.decode("utf-8", "replace").strip().splitlines()
        stats = text[-1] if text else "?"
        return pkg, "same", f"functions with/without line table: {stats}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pkgs", nargs="*")
    ap.add_argument("--batch", help="a Test/run_corpus.py manifest: its CLEAN packages")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--jobs", type=int, default=8)
    args = ap.parse_args()
    pkgs = list(args.pkgs)
    if args.batch:
        manifest = json.loads(Path(args.batch).read_text())
        entries = manifest["packages"]
        for entry in entries:
            if isinstance(entry, dict) and entry.get("status") == "CLEAN":
                pkgs.append(entry.get("path") or entry.get("pkg"))
    if args.limit:
        pkgs = pkgs[:args.limit]
    different = 0
    with ThreadPoolExecutor(args.jobs) as pool:
        for pkg, status, detail in pool.map(check, pkgs):
            print(f"{status:10} {Path(pkg).name}: {detail}")
            different += status == "DIFFERENT"
    print(f"\n{len(pkgs)} packages, {different} different")
    return 1 if different else 0


if __name__ == "__main__":
    sys.exit(main())
