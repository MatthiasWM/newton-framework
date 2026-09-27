#!/usr/bin/env python3
"""Which of newtc's stubs a package needs: a roadmap for running it.

    Test/stub_census.py path/to/newtc path/to/package.pkg [--depth N]

Reads the package's code (newtc -odecompile) and collects what it calls:
global functions (Name(...)), messages (:Name(...), mostly view methods),
and the ROM protos its templates use (_proto: @N). newtc says what each
one is (StubName, Utilities/Unimplemented.h):

  stub         a native newtc doesn't implement yet (NS_STUB)
  native       a native newtc implements
  NewtonScript ROM NewtonScript: its code is read too, down to --depth
               levels (default 2), since that is where many natives are
               really called
  undefined    a global function newtc doesn't have at all
  (a message that isn't a root view method is the app's own or a proto's:
   not listed)

Prints the stubs (with how many of the package's call sites reach them
directly, and which ROM functions reach them), then the undefined ones.
It doesn't run anything: natives that aren't stubs may still be
incomplete (e.g. ones that only print), and what the program really calls
needs a run with -stubs report.
"""

import re
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

KEYWORDS = set("""if then else begin end for foreach do while repeat until loop
return local func call with and or not div mod in to by collect deeply
onexception try inherited self nil true exists constant global""".split())

CALL = re.compile(r"(?<![:.\w?$|'])([A-Za-z_]\w*)\s*\(")
SEND = re.compile(r":\??([A-Za-z_]\w*)\s*\(")
PROTO = re.compile(r"_proto:\s*@(\d+)")
STRING = re.compile(r'"(?:[^"\\]|\\.)*"')


def calls_in(source):
    source = STRING.sub('""', source)    # words in strings aren't calls
    globals_, sends = defaultdict(int), defaultdict(int)
    for m in CALL.finditer(source):
        name = m.group(1)
        if name not in KEYWORDS and name != "DefineGlobalConstant":
            globals_[name] += 1
    for m in SEND.finditer(source):
        sends[m.group(1)] += 1
    return globals_, sends


def run_newtc(newtc, script, *extra):
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "census.ns"
        path.write_text(script)
        return subprocess.run([newtc, "-stubs", "quiet", "-script", str(path), *extra],
                              capture_output=True, text=True).stdout


def classify(newtc, globals_, sends):
    """name -> (kind, stub C name or None), for globals and root view methods."""
    lines = ["DefGlobalVar('census, func(fn, name, what) begin",
             "  if not fn then Print(what & \" \" & name & \" undefined\")",
             "  else if StubName(fn) then Print(what & \" \" & name & \" stub \" & StubName(fn))",
             "  else if IsFunction(fn) and ClassOf(fn) = '_function then Print(what & \" \" & name & \" ns\")",
             "  else Print(what & \" \" & name & \" native\");",
             "end);"]
    for name in globals_:
        lines.append("call census with (GetGlobalFn('|%s|), \"%s\", \"global\");" % (name, name))
    for name in sends:
        lines.append("if HasSlot(@287, '|%s|) then call census with (@287.|%s|, \"%s\", \"method\");"
                     % (name, name, name))
    result = {}
    for line in run_newtc(newtc, "\n".join(lines)).splitlines():
        parts = line.strip().strip('"').split()
        if len(parts) >= 3:
            result[(parts[0], parts[1])] = (parts[2], parts[3] if len(parts) > 3 else None)
    return result


def ns_sources(newtc, keys, protos):
    """The decompiled code of ROM NewtonScript functions and protos."""
    slots = []
    for i, (what, name) in enumerate(keys):
        fn = "GetGlobalFn('|%s|)" % name if what == "global" else "@287.|%s|" % name
        slots.append("f%d: %s" % (i, fn))
    for n in protos:
        slots.append("p%d: Flat(@%d)" % (n, n))
    script = ("func Flat(p) begin local f := {}; local d := 0;\n"
              "  while p and d < 8 do begin\n"
              "    foreach s, v in p do if IsFunction(v) then f.(Intern(\"x\" & d & s)) := v;\n"
              "    p := if HasSlot(p, '_proto) then p._proto else nil; d := d + 1 end;\n"
              "  f end;\n{%s};\n" % ",\n".join(slots))
    return run_newtc(newtc, script, "-decompile")


def main():
    args = sys.argv[1:]
    depth = 2
    if "--depth" in args:
        i = args.index("--depth")
        depth = int(args[i + 1])
        del args[i:i + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    newtc, package = args
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "package.ns"
        subprocess.run([newtc, "-pkg", package, "-odecompile", str(out)], capture_output=True)
        source = out.read_text(errors="replace")

    direct_globals, direct_sends = calls_in(source)
    protos = sorted(set(int(n) for n in PROTO.findall(source)))
    reached_by = defaultdict(set)     # (what, name) -> ROM functions that call it
    known = {}
    frontier_g, frontier_s, frontier_p = dict(direct_globals), dict(direct_sends), protos
    seen_protos = set()
    rom_read = 0
    for level in range(depth + 1):
        kinds = classify(newtc, frontier_g, frontier_s)
        new = {k: v for k, v in kinds.items() if k not in known}
        known.update(new)
        if level == depth:
            break
        ns = [k for k, v in new.items() if v[0] == "ns"]
        todo_protos = [p for p in frontier_p if p not in seen_protos]
        seen_protos.update(todo_protos)
        if not ns and not todo_protos:
            break
        code = ns_sources(newtc, ns, todo_protos)
        rom_read += len(ns) + len(todo_protos)
        g, s = calls_in(code)
        caller = ", ".join(n for _, n in ns[:3]) + (" ..." if len(ns) > 3 else "")
        for name in g:
            reached_by[("global", name)].add("ROM code")
        for name in s:
            reached_by[("method", name)].add("ROM code")
        frontier_g = {n: c for n, c in g.items() if ("global", n) not in known}
        frontier_s = {n: c for n, c in s.items() if ("method", n) not in known}
        frontier_p = sorted(set(int(n) for n in PROTO.findall(code)) - seen_protos)

    stubs = sorted((k, v) for k, v in known.items() if v[0] == "stub")
    undefined = sorted(k for k, v in known.items() if v[0] == "undefined")
    implemented = sum(1 for v in known.values() if v[0] == "native")
    print("%s: %d globals and view methods reached: %d stubs, %d native, %d undefined"
          % (Path(package).name, len(known), len(stubs), implemented, len(undefined)))
    print("(read: the package, and %d ROM functions and protos, %d levels deep)" % (rom_read, depth))
    print("\nstubs (calls in the package itself; 'ROM' if only reached through ROM code):")
    for (what, name), (_, cname) in stubs:
        count = direct_globals.get(name, 0) if what == "global" else direct_sends.get(name, 0)
        print("  %-8s %-28s %-26s %s" % (what, name, cname, count if count else "ROM"))
    if undefined:
        print("\nundefined global functions:")
        for what, name in undefined:
            print("  %-28s %s" % (name, direct_globals.get(name, "ROM")))


if __name__ == "__main__":
    main()
