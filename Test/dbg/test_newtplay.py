#!/usr/bin/env python3
"""NewtPlay (Matt/NewtPlay.h): how it starts.

    test_newtplay.py [--app path/to/NewtPlay.app] [--pkg path/to/a.pkg]

- given a package, it runs it, with its store in
  ~/Library/Application Support/NewtPlay/<package name>/, one per version
  (<name>-v<version>.store; HOME is a temporary folder here);
- started without arguments, it runs the package in its own bundle
  (Contents/Resources/*.nspkg), or waits for the user to choose one (the
  file chooser: it must show, not end at once: it did, before the app had
  finished launching);
- given a file that isn't a Newton package, it says so;
- a package that stops while it starts: NewtPlay says so, and may start it
  again with new data (its store kept as .old-<time>; if it stops again, it
  only says so: NEWTPLAY_TEST_ANSWER answers the alert: 0 Quit, 1 New Data);
- given anything else, it is newtc;
- the Finder's way (LaunchServices, `open`; --no-finder leaves it out): a
  .nspkg opened with it runs (the file comes as an event, not an argument),
  a .newtonpkg opened while it runs gets a NewtPlay of its own, a .pkg
  without quarantine opens with it too (Open With). No quarantined .pkg:
  Gatekeeper would stop it before NewtPlay sees it.
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


# A package whose app stops while it opens (its viewSetupFormScript: an
# index out of bounds, as nBattleship 1.4's with 2.5's settings).
FAILING = r'''
{
  signature: 'package0, id: "xxxx", flags: {noCompression: true}, version: 1,
  copyright: "", name: "failing:SIG", modifyDate: 0, info: "",
  part: [{offset: 0, size: 0, type: "form", flags: {type: 'nos, Notify: true}, info: "",
    data: {app: '|failing:SIG|, text: "Failing",
      theForm: {viewBounds: {left: 0, top: 50, right: 200, bottom: 120}, _proto: @180,
        appSymbol: '|failing:SIG|,
        viewSetupFormScript: func() [][1]},
      installScript: func(part) nil}}]
};
'''


def run(program, args, home, seconds, more_env=None):
    """Run until it ends or for seconds; its stderr and whether it ran on."""
    env = dict(os.environ, HOME=str(home), **(more_env or {}))
    p = subprocess.Popen([str(program), *args], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        out, err = p.communicate(timeout=seconds)
        return out, err, False
    except subprocess.TimeoutExpired:
        p.kill()
        out, err = p.communicate()
        return out, err, True


def finder_checks(app, program, pkg, tmp):
    """Open packages as the Finder does (open -a), HOME a temporary folder."""
    import time
    def count():
        p = subprocess.run(["pgrep", "-f", f"{app}/Contents/MacOS/NewtPlay"], capture_output=True, text=True)
        return len(p.stdout.split())
    def stores():
        return sorted(p.name for p in (home / "Library" / "Application Support" / "NewtPlay").rglob("*.store"))
    def wait(test, seconds=10):
        end = time.time() + seconds
        while time.time() < end and not test():
            time.sleep(0.25)
        return test()
    home = tmp / "finder-home"
    (home / "Library" / "Application Support").mkdir(parents=True)
    first = tmp / "first.nspkg"
    shutil.copy(pkg, first)
    second = tmp / "second.newtonpkg"
    subprocess.run([str(program), "-script", str(tmp / "failing.ns"), "-opkg", str(second)], capture_output=True)
    subprocess.run([str(program), "-hello", "-opkg", str(tmp / "hello2.pkg")], capture_output=True)
    plain = tmp / "plain.pkg"   # a .pkg without quarantine
    shutil.copy(tmp / "hello2.pkg", plain)
    subprocess.run(["xattr", "-c", str(first), str(second), str(plain)])
    subprocess.run(["pkill", "-f", f"{app}/Contents/MacOS/NewtPlay"])
    checks = []
    env = ["--env", f"HOME={home}", "--env", "NEWTPLAY_TEST_ANSWER=0"]
    subprocess.run(["open", "-n", "-a", str(app), *env, str(first)])
    ok = wait(lambda: count() == 1 and len(stores()) == 1)
    checks.append(("Finder: a .nspkg opened with NewtPlay runs", ok, (count(), stores())))
    subprocess.run(["open", "-a", str(app), str(second)], capture_output=True)
    ok = wait(lambda: len(stores()) == 2)
    checks.append(("Finder: a .newtonpkg opened while one runs: a NewtPlay of its own (its store)", ok, stores()))
    subprocess.run(["pkill", "-f", f"{app}/Contents/MacOS/NewtPlay"])
    wait(lambda: count() == 0, 5)
    shutil.rmtree(home / "Library" / "Application Support" / "NewtPlay", ignore_errors=True)
    subprocess.run(["open", "-n", "-a", str(app), *env, str(plain)])
    ok = wait(lambda: count() == 1 and len(stores()) == 1)
    checks.append(("Finder: a .pkg without quarantine opened with NewtPlay runs", ok, (count(), stores())))
    subprocess.run(["pkill", "-f", f"{app}/Contents/MacOS/NewtPlay"])
    return checks


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", default=str(ROOT / "build" / "VSCode" / "NewtPlay.app"))
    ap.add_argument("--pkg")
    ap.add_argument("--no-finder", action="store_true", help="leave out the tests through LaunchServices")
    args = ap.parse_args()
    app = Path(args.app).resolve()   # (open -a wants a whole path)
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
        checks.append(("a package given: it runs, its store in NewtPlay/<name>/<name>-v<version>.store",
                       ran_on and len(made) == 1 and "-v" in made[0], made))

        out, _, _ = run(program, ["-s", "Print(6 * 7);"], home, 20)
        checks.append(("anything else: newtc", out.strip().endswith("42"), out.strip()[-20:]))

        _, err, ran_on = run(program, [], home, 3)
        checks.append(("no arguments, no package in the bundle: it waits for a choice", ran_on, err.strip()[:80]))

        fake = tmp / "fake.pkg"
        fake.write_text("not a package")
        _, err, _ = run(program, [str(fake)], home, 3)
        checks.append(("not a Newton package: it says so", "not a Newton package" in err, err.strip()[:80]))

        failing = tmp / "failing.ns"
        failing.write_text(FAILING)
        failing_pkg = tmp / "failing.pkg"
        subprocess.run([str(program), "-script", str(failing), "-opkg", str(failing_pkg)], capture_output=True, check=True)
        shutil.rmtree(stores, ignore_errors=True)
        env_answer = {"NEWTPLAY_TEST_ANSWER": "0"}
        _, err, ran_on = run(program, [str(failing_pkg)], home, 20, env_answer)
        checks.append(("a package that stops: it says so (Quit)", "failing:SIG stopped: " in err and not ran_on, err.strip()[-100:]))
        _, err, ran_on = run(program, [str(failing_pkg)], home, 20, {"NEWTPLAY_TEST_ANSWER": "1"})
        old = sorted(p.name for p in stores.rglob("*.old-*")) if stores.exists() else []
        checks.append(("... with new data: the store kept, started again, and then only said",
                       len(old) == 1 and "stopped also with new data" in err and not ran_on, (old, err.strip()[-100:])))

        copy = tmp / "Copy" / "NewtPlay.app"
        shutil.copytree(app, copy, symlinks=True)
        shutil.copy(pkg, copy / "Contents" / "Resources" / "App.nspkg")
        subprocess.run(["xattr", "-cr", str(copy)], check=True)
        subprocess.run(["codesign", "--force", "--sign", "-", str(copy)], capture_output=True, check=True)
        shutil.rmtree(stores, ignore_errors=True)
        _, err, ran_on = run(copy / "Contents" / "MacOS" / "NewtPlay", [], home, 4)
        made = sorted(p.name for p in stores.rglob("*.store")) if stores.exists() else []
        checks.append(("no arguments: the package in its bundle runs", ran_on and len(made) == 1, made))
        if not args.no_finder:
            checks += finder_checks(app, program, pkg, tmp)
    failed = 0
    for name, ok, detail in checks:
        print(("ok      " if ok else "FAIL    ") + name + ("" if ok else f"  ({detail})"))
        failed += not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
