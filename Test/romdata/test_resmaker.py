#!/usr/bin/env python3
"""
The ROM data (ROMData/forNewton, ROMData/forNTK) is ResMaker's output and
nothing else:
  - ResMaker, run as the comments in ROMData/ResMaker/ResMaker.cc say, makes
    exactly the files in the repo, for Newton and for NTK (skipped when
    ResMaker isn't built: cmake --build <build> --target ResMaker)
  - B52: RSspace is the string " " (it was U+2000: byte-swapped)
  - B53: the ROM's largest integer, 536870911 (Ref 0x7FFFFFFC), stays that
    (ResMaker took it for negative: -536870913)

Usage: Test/romdata/test_resmaker.py [--resmaker path]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import filecmp
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ROMDATA = REPO / "ROMData"
FILES = ["ROMObjects.h", "ROMSymbols.h", "ROMSymbols.c", "ROMData.c", "ROMFunctions.c"]
RUNS = {
    "forNewton": [],
    "forNTK": ["-via", "../Toolkit.exp", "-via", "../Editor.exp"],
}

failed = 0


def check(ok, text):
    global failed
    print(("ok      " if ok else "FAIL    ") + text)
    failed += not ok


def member(source, label):
    """The initializer of gROMData's member <label>, as text."""
    m = re.search(r"^\t\.%s = \{(.*?)\},\n\t\." % re.escape(label), source, re.S | re.M)
    return m.group(1) if m else ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--resmaker", default=str(REPO / "build" / "VSCode" / "ResMaker"))
    args = ap.parse_args()

    for name in RUNS:
        source = (ROMDATA / name / "ROMData.c").read_text()
        space = member(source, "space")
        check("{' ',0}" in space and "0x2000" not in space,
              "%s: RSspace is \" \" (B52)" % name)
        check("NR_REF(0x7FFFFFFC)" in source and "0x80000004" not in source,
              "%s: 536870911 is 536870911, not -536870913 (B53)" % name)

    resmaker = Path(args.resmaker).resolve()     # it runs in another folder
    if not resmaker.exists():
        print("skip    ResMaker isn't built (%s): output not compared" % resmaker)
        return 1 if failed else 0

    with tempfile.TemporaryDirectory() as tmp:
        # ResMaker also writes ../Frames/RSData.h and RSSymbols.h (from
        # ROMData): run it in a copy of the layout it expects
        work = Path(tmp)
        (work / "ROMData").mkdir()
        (work / "Frames").mkdir()
        for f in ["rom.image", "RefStarSource.txt", "717006.cfunctions", "NTKFunctions.newtonpkg"]:
            shutil.copy(ROMDATA / f, work / "ROMData" / f)
        for f in ["Toolkit.exp", "Editor.exp"]:
            shutil.copy(REPO / f, work / f)
        for name, options in RUNS.items():
            out = work / "ROMData" / name
            out.mkdir()
            run = subprocess.run([str(resmaker), "rom.image", "-s", "RefStarSource.txt", *options,
                                  "-sym", "717006.cfunctions", "-o", name],
                                 cwd=work / "ROMData", capture_output=True, text=True, timeout=600)
            check(run.returncode == 0 and "Sanitizer" not in run.stderr,
                  "%s: ResMaker runs (exit %d)" % (name, run.returncode))
            for f in FILES:
                check(filecmp.cmp(out / f, ROMDATA / name / f, shallow=False),
                      "%s: ResMaker makes %s/%s as it is in the repo" % (name, name, f))
        for f in ["RSData.h", "RSSymbols.h"]:
            check(filecmp.cmp(work / "Frames" / f, REPO / "Frames" / f, shallow=False),
                  "ResMaker makes Frames/%s as it is in the repo" % f)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
