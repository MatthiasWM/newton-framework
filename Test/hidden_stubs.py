#!/usr/bin/env python3
"""Hidden stubs: natives that do nothing, but aren't NS_STUBs, so that
`newtc -stubs report` can't see them (Utilities/Unimplemented.h).

    Test/hidden_stubs.py [--all]

Takes every native in newtc's sources (a C function Ref F...(RefArg rcvr, ...),
as the ROM's tables name them; NS/plainC.txt lists the ROM's plain C ones),
and prints those whose body only returns
a constant (nil, true, a number) without using its arguments, or prints
and returns, or says it isn't done (TODO, "not implemented", "unimplemented").
--all also lists the natives of NS/plainC.txt it can't find.
Some of them are right to do nothing (e.g. hardware newtc doesn't have):
the list is for reading, one by one.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP = {"build", "Test", ".git", "Einstein"}

DEFINITION = re.compile(r"^\s*(?:extern\s+\"C\"\s+)?Ref\s+(F\w+)\s*\(([^)]*)\)\s*(?://[^\n]*)?(\{|$)", re.M)
STUB = re.compile(r"NS_STUB(?:_NIL_OK)?\s*\(\s*(\w+)")
PRINTS = re.compile(r"\b(printf|fprintf|puts|REPprintf|std::cout|std::cerr|cerr|cout)\b")
NOT_DONE = re.compile(r"\b(TODO|FIXME|NYI)\b|(?i:not implemented|unimplemented|not yet)")
# Natives looked at that are right to do what they do (10.7g, 2026-09-29).
REVIEWED = {
    "FBatteryCount": "one battery, as a MessagePad",
    "FSoundCheck": "does nothing in the ROM too",
}
CONSTANT = re.compile(r"^return\s+(NILREF|TRUEREF|FALSEREF|RA\(NILREF\)|RA\(TRUEREF\)|MAKEINT\(-?\d+\)|MAKEBOOLEAN\([^)]*\)|kNoErr|noErr|0)\s*;$")


def natives():
    text = (ROOT / "NS" / "plainC.txt").read_text(errors="replace")
    return sorted(set(re.findall(r"name: '(F\w+)", text)))


def sources():
    """The files newtc is built from (build/VSCode/compile_commands.json;
    not Views/ and the other MessagePad-only code), else all of them."""
    commands = ROOT / "build" / "VSCode" / "compile_commands.json"
    if commands.exists():
        import json
        files = {Path(c["file"]).resolve() for c in json.loads(commands.read_text())
                 if "/newtc.dir/" in c.get("output", c.get("command", ""))}
        if files:
            yield from sorted(p for p in files if p.suffix in (".cc", ".cpp", ".c", ".mm") and ROOT in p.parents)
            return
    for path in ROOT.rglob("*"):
        if path.suffix in (".cc", ".cpp", ".c", ".mm") and not (set(path.relative_to(ROOT).parts) & SKIP):
            yield path


def strip_comments(code):
    code = re.sub(r"/\*.*?\*/", " ", code, flags=re.S)
    return re.sub(r"//[^\n]*", " ", code)


def body_at(text, start):
    """The function body from the { at or after start (brace matching)."""
    open_ = text.index("{", start)
    depth = 0
    for i in range(open_, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[open_ + 1:i]
    return None


def classify(params, body):
    code = strip_comments(body)
    statements = [s.strip() + ";" for s in code.split(";") if s.strip()]
    names = [p.strip().split()[-1].lstrip("*&") for p in params.split(",") if p.strip() and p.strip() != "void"]
    uses_args = any(re.search(r"\b" + re.escape(n) + r"\b", code) for n in names if n not in ("rcvr", "inRcvr"))
    reasons = []
    if NOT_DONE.search(body):
        reasons.append("says it isn't done")
    if PRINTS.search(code) and len(statements) <= 3 and not uses_args:
        reasons.append("prints")
    if len(statements) == 1 and CONSTANT.match(statements[0]) and not uses_args:
        reasons.append("only " + statements[0])
    return reasons


def main():
    show_all = "--all" in sys.argv
    table = set(natives())
    found, stubs, stub_names = {}, set(), set()
    for path in sources():
        text = path.read_text(errors="replace")
        for m in STUB.finditer(text):
            stub_names.add(m.group(1))
            stubs.add(m.group(1).lower())
            stubs.add("f" + m.group(1).lower())   # NS_STUB(FormatVertical, ...): the ROM's FFormatVertical
        for m in DEFINITION.finditer(text):
            name = m.group(1)
            if name in found or not m.group(2).strip().startswith("RefArg"):
                continue
            body = body_at(text, m.start(3) if m.group(3) else m.end())
            if body is not None:
                line = text.count("\n", 0, m.start()) + 1
                found[name] = (path.relative_to(ROOT), line, m.group(2), body)
    hidden = []
    for name in sorted(found):
        if name.lower() in stubs or name in REVIEWED:
            continue
        path, line, params, body = found[name]
        reasons = classify(params, body)
        if reasons:
            hidden.append((name, path, line, reasons))
    for name, path, line, reasons in hidden:
        print(f"{name:32} {path}:{line}  {'; '.join(reasons)}")
    # (C names differ in capitals from the ROM's at times: FSubstr, 'FSubStr)
    defined = {n.lower() for n in found} | stubs
    missing = sorted(n for n in table if n.lower() not in defined)
    print(f"\n{len(hidden)} hidden stubs? of {len(found)} natives defined "
          f"({len(stub_names)} NS_STUBs besides; of NS/plainC.txt's {len(table)}, {len(missing)} not found)")
    if show_all:
        print("not found:", " ".join(missing))
    return 0


if __name__ == "__main__":
    sys.exit(main())
