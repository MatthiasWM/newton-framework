#!/usr/bin/env python3
"""NewtPlay (Matt/NewtPlay.h): how it starts.

    test_newtplay.py [--app path/to/NewtPlay.app] [--pkg path/to/a.pkg]

- given a package, it runs it, with its store in
  ~/Library/Application Support/NewtPlay/<package name>/ (HOME is a temporary
  folder here);
- started without arguments, it runs the package in its own bundle
  (Contents/Resources/*.nspkg);
- given a file that isn't a Newton package, it says so;
- given anything else, it is newtc.
A running package keeps its window open: the test stops it after a while.
Default app: build/VSCode/NewtPlay.app (cmake --build build/VSCode --target
NewtPlay); default package: newtc's Hello app, written with newtc -hello.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def run(program, args, home, seconds):
    """Run until it ends or for seconds; its stderr and whether it ran on."""
    env = dict(os.environ, HOME=str(home))
    p = subprocess.Popen([str(program), *args], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        out, err = p.communicate(timeout=seconds)
        return out, err, False
    except subprocess.TimeoutExpired:
        p.kill()
        out, err = p.communicate()
        return out, err, True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", default=str(ROOT / "build" / "VSCode" / "NewtPlay.app"))
    ap.add_argument("--pkg")
    args = ap.parse_args()
    app = Path(args.app)
    program = app / "Contents" / "MacOS" / "NewtPlay"
    if not program.exists():
        print(f"no {program} (cmake --build build/VSCode --target NewtPlay)")
        return 1
    checks = []
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        home = tmp / "home"
        (home / "Library" / "Application Support").mkdir(parents=True)
        pkg = Path(args.pkg) if args.pkg else tmp / "hello.pkg"
        if not args.pkg:
            subprocess.run([str(program), "-hello", "-opkg", str(pkg)], capture_output=True, check=True)
        stores = home / "Library" / "Application Support" / "NewtPlay"

        _, err, ran_on = run(program, [str(pkg)], home, 4)
        made = sorted(p.relative_to(stores).as_posix() for p in stores.rglob("*.store")) if stores.exists() else []
        checks.append(("a package given: it runs, its store in NewtPlay/<name>/", ran_on and len(made) == 1, made))

        out, _, _ = run(program, ["-s", "Print(6 * 7);"], home, 20)
        checks.append(("anything else: newtc", out.strip().endswith("42"), out.strip()[-20:]))

        fake = tmp / "fake.pkg"
        fake.write_text("not a package")
        _, err, _ = run(program, [str(fake)], home, 3)
        checks.append(("not a Newton package: it says so", "not a Newton package" in err, err.strip()[:80]))

        copy = tmp / "Copy" / "NewtPlay.app"
        shutil.copytree(app, copy, symlinks=True)
        shutil.copy(pkg, copy / "Contents" / "Resources" / "App.nspkg")
        subprocess.run(["xattr", "-cr", str(copy)], check=True)
        subprocess.run(["codesign", "--force", "--sign", "-", str(copy)], capture_output=True, check=True)
        shutil.rmtree(stores, ignore_errors=True)
        _, err, ran_on = run(copy / "Contents" / "MacOS" / "NewtPlay", [], home, 4)
        made = sorted(p.name for p in stores.rglob("*.store")) if stores.exists() else []
        checks.append(("no arguments: the package in its bundle runs", ran_on and len(made) == 1, made))
    failed = 0
    for name, ok, detail in checks:
        print(("ok      " if ok else "FAIL    ") + name + ("" if ok else f"  ({detail})"))
        failed += not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
