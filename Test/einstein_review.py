#!/usr/bin/env python3
"""
Test/einstein_review.py -- Tier 3 ground-truth review helper.

'/Users/matt/dev/Einstein/Sample Code/' holds ~86 official Apple DTS sample
projects, most shipping *both* a compiled `.pkg` and genuine human-written
NewtonScript source (`.text`) in the same directory under the same
basename (e.g. `ChezDTS.pkg` + `ChezDTS.text`) -- real ground truth, not
just plausible-looking decompiler output. Matt has been using these
manually already (`./testdec '<pkg>'` + `open -a xcode '<matching .text>'`,
recorded ad hoc at the top of Matt/Decompiler.cc). This script scripts that
habit instead of retyping two paths by hand each time, and tracks review
status across runs in a checklist file so review work accumulates instead
of restarting from scratch every session.

What it does NOT do: automatically decide pass/fail. NTK's own `.text`
export uses auto-generated view names (`_view000`, ...) and differs
stylistically from our decompiler's output, so exact text diffing isn't a
reliable oracle here -- this tool's job is to make the side-by-side
comparison fast and to remember what's already been checked, not to judge
it for you.

Usage:
    python3 Test/einstein_review.py                  # decompile everything, list what needs review
    python3 Test/einstein_review.py --open-next       # open the next un-reviewed pair in Xcode
    python3 Test/einstein_review.py --mark ChezDTS-2/ChezDTS match --notes "..."
    python3 Test/einstein_review.py --mark ChezDTS-2/ChezDTS mismatch --notes "..."

Run from the repo root.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

EINSTEIN_ROOT = Path("/Users/matt/dev/Einstein/Sample Code")
CHECKLIST_PATH = Path("Test/einstein_checklist.json")
REVIEW_DIR = Path("Test/corpus_results/einstein_review")

SYNTAX_ERROR_MARKERS = ("!!! Exception:", "syntax error")


def find_pairs():
    """Every (pkg, text) pair under EINSTEIN_ROOT sharing a directory and
    basename. `key` is the path relative to EINSTEIN_ROOT with no
    extension, e.g. "Application Design/ChezDTS-2/ChezDTS" -- stable across
    runs, used as the checklist key."""
    pairs = []
    for pkg in sorted(EINSTEIN_ROOT.rglob("*.pkg")):
        text = pkg.with_suffix(".text")
        if text.exists():
            key = str(pkg.relative_to(EINSTEIN_ROOT).with_suffix(""))
            pairs.append((key, pkg, text))
    return pairs


def classify(output: str) -> str:
    if any(m in output for m in SYNTAX_ERROR_MARKERS):
        return "CRASHED"
    if "WARNING:" in output and "unresolved nodes" in output:
        return "UNRESOLVED"
    return "CLEAN"


def decompile(newtc: str, pkg: Path, timeout: float) -> tuple[str, str]:
    try:
        proc = subprocess.run(
            [newtc, "-pkg", str(pkg), "-decompile"],
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            timeout=timeout, encoding="utf-8", errors="replace",
        )
    except subprocess.TimeoutExpired:
        return "TIMEOUT", ""
    out = proc.stdout or ""
    if proc.returncode != 0:
        return "CRASHED", out
    return classify(out), out


def load_checklist():
    if CHECKLIST_PATH.exists():
        return json.loads(CHECKLIST_PATH.read_text())
    return {}


def save_checklist(checklist):
    CHECKLIST_PATH.write_text(json.dumps(checklist, indent=2, sort_keys=True))


def sanitize(key: str) -> str:
    return key.replace("/", "__").replace(" ", "_")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--newtc", default="build/VSCode/newtc")
    ap.add_argument("--timeout", default=20.0, type=float)
    ap.add_argument("--open-next", action="store_true",
                     help="open the decompiled output and matching .text for the "
                          "first pair that has never been marked, in Xcode")
    ap.add_argument("--mark", nargs=2, metavar=("KEY", "VERDICT"),
                     help="record a review verdict for one pair, e.g. "
                          "--mark 'Application Design/ChezDTS-2/ChezDTS' match")
    ap.add_argument("--notes", default="", help="notes to attach with --mark")
    ap.add_argument("--skip-decompile", action="store_true",
                     help="don't re-run newtc, just operate on the existing checklist")
    args = ap.parse_args()

    checklist = load_checklist()

    if args.mark:
        key, verdict = args.mark
        if key not in checklist:
            sys.exit(f"Unknown key {key!r} -- run without --mark first to populate the checklist")
        checklist[key]["review"] = verdict
        checklist[key]["notes"] = args.notes
        save_checklist(checklist)
        print(f"Marked {key!r} as {verdict!r}")
        return

    newtc = str(Path(args.newtc).resolve())
    if not args.skip_decompile and not Path(newtc).exists():
        sys.exit(f"newtc binary not found at {newtc}")

    pairs = find_pairs()
    print(f"Found {len(pairs)} .pkg/.text pairs under {EINSTEIN_ROOT}", file=sys.stderr)
    REVIEW_DIR.mkdir(parents=True, exist_ok=True)

    if not args.skip_decompile:
        for key, pkg, text in pairs:
            status, out = decompile(newtc, pkg, args.timeout)
            entry = checklist.setdefault(key, {"review": "unreviewed", "notes": ""})
            entry["decompile_status"] = status
            entry["pkg"] = str(pkg)
            entry["text"] = str(text)
            out_dir = REVIEW_DIR / sanitize(key)
            out_dir.mkdir(parents=True, exist_ok=True)
            (out_dir / "decompiled.txt").write_text(out, errors="replace")
            entry["decompiled_copy"] = str(out_dir / "decompiled.txt")
        save_checklist(checklist)

    by_review = {}
    for key, entry in checklist.items():
        by_review.setdefault(entry.get("review", "unreviewed"), []).append(key)

    print(f"\nTotal pairs tracked: {len(checklist)}")
    for review, keys in sorted(by_review.items()):
        print(f"  {review}: {len(keys)}")
    by_decompile = {}
    for entry in checklist.values():
        by_decompile.setdefault(entry.get("decompile_status", "?"), 0)
        by_decompile[entry.get("decompile_status", "?")] += 1
    print("\nDecompile status:")
    for status, n in sorted(by_decompile.items()):
        print(f"  {status}: {n}")

    unreviewed = by_review.get("unreviewed", [])
    if args.open_next:
        if not unreviewed:
            print("\nNothing left to review.")
            return
        key = sorted(unreviewed)[0]
        entry = checklist[key]
        print(f"\nOpening {key!r} for review...")
        subprocess.run(["open", "-a", "Xcode", entry["decompiled_copy"]])
        subprocess.run(["open", "-a", "Xcode", entry["text"]])
        print(f"When done: python3 {sys.argv[0]} --mark {key!r} match|mismatch --notes '...'")
    else:
        print(f"\n{len(unreviewed)} pairs still unreviewed. Use --open-next to review the next one.")


if __name__ == "__main__":
    main()
