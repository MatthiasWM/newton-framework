#!/usr/bin/env python3
"""
newton-framework as a subproject (NCX's step 0.2): Test/cmake/subproject
pulls this checkout in with FetchContent, as NCX does.
  - by default the tools, the sanitizer and MessagePad are off: the project
    configures and builds, defines none of the tools' targets, fetches
    nothing (no FLTK)
  - a parent that sets NEWTON_BUILD_TOOLS ON (a normal variable) gets them;
    with --build-tools they are built there too (minutes: newtc, newtdump,
    whose protocol extension newtc compiles while building)

Usage: Test/cmake/test_subproject.py [--generator Ninja] [--build-tools]
Exit code 0 if it passes, 1 if not.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PARENT = REPO / "Test" / "cmake" / "subproject"
TOOLS = ["newtc", "newtdump", "newtdump-test-newton", "ResMaker", "MessagePad"]

failed = 0


def check(ok, text, detail=""):
    global failed
    print(("ok      " if ok else "FAIL    ") + text)
    if not ok and detail:
        print(detail[-3000:])
    failed += not ok


def configure(build, generator, *defines):
    return subprocess.run(["cmake", "-S", str(PARENT), "-B", str(build), "-G", generator,
                           "-DFETCHCONTENT_SOURCE_DIR_NEWTON=" + str(REPO), *defines],
                          capture_output=True, text=True, timeout=600)


def targets(build):
    run = subprocess.run(["cmake", "--build", str(build), "--target", "help"],
                         capture_output=True, text=True, timeout=120)
    names = set()
    for line in run.stdout.splitlines():
        name = line.split(":")[0].strip().lstrip(".").strip()
        if name:
            names.add(name.split("/")[-1])
    return names


def cache(build):
    values = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if "=" in line and not line.startswith(("#", "//")):
            key, value = line.split("=", 1)
            values[key.split(":")[0]] = value
    return values


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--generator", default="Ninja")
    ap.add_argument("--build-tools", action="store_true")
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        build = Path(tmp) / "default"
        run = configure(build, args.generator)
        check(run.returncode == 0, "configures as a subproject", run.stdout + run.stderr)
        if run.returncode == 0:
            values = cache(build)
            check(values.get("NEWTON_BUILD_TOOLS") == "OFF" and values.get("NEWTON_SANITIZE") == "OFF"
                  and values.get("NEWTON_BUILD_MESSAGEPAD") == "OFF",
                  "tools, sanitizer, MessagePad off by default")
            names = targets(build)
            check(not any(t in names for t in TOOLS), "none of the tools' targets",
                  " ".join(sorted(names)))
            check(not (build / "_deps" / "fltk-src").exists(), "nothing fetched (no FLTK)")
            run = subprocess.run(["cmake", "--build", str(build)], capture_output=True, text=True, timeout=1800)
            check(run.returncode == 0, "builds", run.stdout + run.stderr)

        build = Path(tmp) / "tools"
        run = configure(build, args.generator, "-DTEST_TOOLS=ON")
        check(run.returncode == 0, "configures with the tools", run.stdout + run.stderr)
        if run.returncode == 0:
            names = targets(build)
            check({"newtc", "newtdump", "ResMaker"} <= names,
                  "the parent's NEWTON_BUILD_TOOLS ON gives newtc, newtdump, ResMaker",
                  " ".join(sorted(names)))
            check(cache(build).get("NEWTON_SANITIZE") == "OFF", "... still without the sanitizer")
            if args.build_tools:
                run = subprocess.run(["cmake", "--build", str(build), "--target", "newtc", "newtdump"],
                                     capture_output=True, text=True, timeout=3600)
                check(run.returncode == 0, "... and builds newtc and newtdump there", run.stdout + run.stderr)
                newtc = next(build.rglob("newtc"), None) if run.returncode == 0 else None
                if newtc:
                    run = subprocess.run([str(newtc), "-s", "Print(1 + 1)"], capture_output=True, text=True, timeout=60)
                    check(run.stdout.strip() == "2", "... and that newtc runs", run.stdout + run.stderr)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
