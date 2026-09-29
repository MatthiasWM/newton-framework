#!/usr/bin/env python3
"""NewtPlay (Matt/NewtPlay.h): how it starts.

    test_newtplay.py [--app path/to/NewtPlay.app] [--pkg path/to/a.pkg]

- given a package, it runs it, with its store in
  ~/Library/Application Support/NewtPlay/<package name>/, one per version
  (<name>-v<version>.store; HOME is a temporary folder here);
- started without arguments, it runs the package in its own bundle
  (Contents/Resources/*.nspkg), or shows its splash window and waits for
  the user to choose one (it must not end at once: it did, before the app
  had finished launching); NEWTPLAY_TEST_SPLASH saves a picture of it;
- the packages run go into its history (Fl_Preferences:
  ~/Library/Preferences/newton-framework.org/NewtPlay.prefs), newest first;
- given a file that isn't a Newton package, it says so;
- a package that stops while it starts: NewtPlay says so, and may start it
  again with new data (its store kept as .old-<time>; if it stops again, it
  only says so: NEWTPLAY_TEST_ANSWER answers the alert: 0 Quit, 1 New Data);
- given anything else, it is newtc;
- -make-shortcut and -make-app (the splash window's and the File menu's
  Make Shortcut and Make App): <name>.app next to the package, with the
  package, its icon, an Info.plist of its own, signed ad hoc; one replaces
  the other if the user says so (NEWTPLAY_TEST_ANSWER: 0 Cancel, 1
  Replace); made from the package in an app, it replaces that app; the app
  runs its package;
- the Finder's way (LaunchServices, `open`; --no-finder leaves it out): a
  .nspkg opened with it runs (the file comes as an event, not an argument),
  a .newtonpkg opened while it runs gets a NewtPlay of its own, a .pkg
  without quarantine opens with it too (Open With), a shortcut runs its
  package (with a NewtPlay LaunchServices knows). No quarantined .pkg:
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
LSREGISTER = ("/System/Library/Frameworks/CoreServices.framework/Frameworks/"
              "LaunchServices.framework/Support/lsregister")


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

# A package with an icon (8 by 8 pixels: a frame and a cross) and a window.
ICON = r'''
{
  signature: 'package0, id: "xxxx", flags: {noCompression: true}, version: 3,
  copyright: "", name: "icon:SIG", modifyDate: 0, info: "",
  part: [{offset: 0, size: 0, type: "form", flags: {type: 'nos, Notify: true}, info: "",
    data: {app: '|icon:SIG|, text: "Icon",
      icon: {bounds: {left: 0, top: 0, right: 8, bottom: 8},
        bits: MakeBinaryFromHex("0000000000040000000000000008000"
          & "8" & "FF000000C3000000A500000099000000" & "99000000A5000000C3000000FF000000", 'bits)},
      theForm: {viewBounds: {left: 0, top: 50, right: 200, bottom: 120}, _proto: @180,
        appSymbol: '|icon:SIG|},
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
    # a shortcut: its script opens NewtPlay (-b: any LaunchServices knows)
    shutil.rmtree(home / "Library" / "Application Support" / "NewtPlay", ignore_errors=True)
    subprocess.run([str(program), "-make-shortcut", str(first)], capture_output=True)
    shortcut = tmp / "first.app"
    subprocess.run([str(shortcut / "Contents" / "MacOS" / "first")], env=dict(os.environ, HOME=str(home)))
    ok = wait(lambda: len(stores()) == 1)
    checks.append(("Finder: a shortcut runs its package with NewtPlay", ok, stores()))
    subprocess.run(["pkill", "-f", str(shortcut)])
    subprocess.run([LSREGISTER, "-u", str(shortcut)], capture_output=True)
    return checks


def make_checks(program, pkg, tmp, home):
    """-make-shortcut, -make-app: the bundles, replacing, icons, running."""
    checks = []
    folder = tmp / "Made"
    folder.mkdir()
    source = folder / "Icon.nspkg"
    (tmp / "icon.ns").write_text(ICON)
    subprocess.run([str(program), "-script", str(tmp / "icon.ns"), "-opkg", str(source)], capture_output=True, check=True)
    bundle = folder / "Icon.app"
    def make(kind, package, answer="1"):
        env = {"NEWTPLAY_TEST_ANSWER": answer}
        out, err, _ = run(program, [f"-make-{kind}", str(package)], home, 60, env)
        return out.strip(), err.strip()
    def plist():
        p = bundle / "Contents" / "Info.plist"
        return p.read_text() if p.exists() else ""
    def signed():
        return subprocess.run(["codesign", "--verify", "--strict", str(bundle)], capture_output=True).returncode == 0
    def program_of():
        p = bundle / "Contents" / "MacOS" / "Icon"
        return p.read_bytes()[:4] if p.exists() else b""

    out, err = make("shortcut", source)
    ok = (out == str(bundle) and (bundle / "Contents" / "Resources" / "Icon.nspkg").read_bytes() == source.read_bytes()
          and "org.newton-framework.shortcut.Icon" in plist() and program_of() == b"#!/b" and signed())
    checks.append(("-make-shortcut: <name>.app next to the package, a script, its own ID, signed", ok, (out, err)))
    icns = bundle / "Contents" / "Resources" / "AppIcon.icns"
    checks.append(("... the package's icon as its icon", icns.exists() and icns.stat().st_size > 1000
                   and "AppIcon" in plist(), icns.exists()))
    out, err = make("app", source, "0")
    checks.append(("-make-app over a shortcut, cancelled: the shortcut stays", out == "" and program_of() == b"#!/b",
                   (out, err)))
    out, err = make("app", source, "1")
    ok = (out == str(bundle) and program_of() == b"\xcf\xfa\xed\xfe" or program_of() == b"\xca\xfe\xba\xbe")
    checks.append(("... replaced: an app (NewtPlay's program), its own ID, signed",
                   ok and "org.newton-framework.app.Icon" in plist() and signed(), (out, err, program_of())))
    shutil.rmtree(home / "Library" / "Application Support" / "NewtPlay", ignore_errors=True)
    _, err, ran_on = run(bundle / "Contents" / "MacOS" / "Icon", [], home, 6)
    made = sorted(p.name for p in (home / "Library").rglob("*.store"))
    checks.append(("... the app runs its package", ran_on and made == ["icon_SIG-v3.store"], (made, err[-200:])))
    out, err = make("shortcut", bundle / "Contents" / "Resources" / "Icon.nspkg")
    checks.append(("made from the package in an app: it replaces the app",
                   out == str(bundle) and program_of() == b"#!/b" and signed()
                   and (bundle / "Contents" / "Resources" / "Icon.nspkg").read_bytes() == source.read_bytes(), (out, err)))
    _, err = make("app", tmp / "failing.ns")
    checks.append(("not a package: it says so", "not a Newton package" in err, err))
    subprocess.run([LSREGISTER, "-u", str(bundle)], capture_output=True)
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
        splash = tmp / "splash.png"
        run(program, [], home, 20, {"NEWTPLAY_TEST_SPLASH": str(splash)})
        checks.append(("... in its splash window (a picture of it)", splash.exists() and splash.stat().st_size > 1000,
                       splash.exists()))
        prefs = home / "Library" / "Preferences" / "newton-framework.org" / "NewtPlay.prefs"
        text = prefs.read_text() if prefs.exists() else ""
        checks.append(("the package run is in the history", "path:" in text and pkg.name in text.replace("\n+", ""),
                       text[-200:]))

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
        # (a copy that ran is registered with LaunchServices: not any more)
        subprocess.run([LSREGISTER, "-u", str(copy)], capture_output=True)
        checks += make_checks(program, pkg, tmp, home)
        if not args.no_finder:
            checks += finder_checks(app, program, pkg, tmp)
    failed = 0
    for name, ok, detail in checks:
        print(("ok      " if ok else "FAIL    ") + name + ("" if ok else f"  ({detail})"))
        failed += not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
