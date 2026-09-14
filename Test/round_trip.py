#!/usr/bin/env python3
"""
Test/round_trip.py -- Tier 2 self-consistency round-trip check.

Formalizes the idea already sketched (but never finished) in the repo-root
`testdec` script and its CMakeLists.txt comment ("Testing is decompiling and
recompiling builds the same object tree"). Decompile a package, recompile
the decompiled source with our own from-scratch compiler, decompile that,
recompile again, decompile again:

    gen1_src = decompile(pkg)              -- generation 1
    gen2_src = decompile(compile(gen1_src)) -- generation 2
    gen3_src = decompile(compile(gen2_src)) -- generation 3

The invariant this checks is gen2_src == gen3_src: once a program has gone
through our own compiler once, decompiling it and recompiling it again
should reach a fixed point, since both ends of that second round-trip are
entirely our own code (no NTK optimizer involved on either side). This
holds regardless of whether the *original* package was built by NTK's
optimizing compiler -- gen1_src vs gen2_src is reported too, but a mismatch
there is only informative (NTK doesn't optimize the way our compiler
doesn't optimize, so first-generation differences are expected for real
packages), never a failure on its own.

A gen2/gen3 mismatch is a strong, NTK-independent signal of a genuine
decompiler bug -- including a semantically *wrong but fully-resolved*
result (e.g. swapped operands, inverted branch polarity), which Tier 1
(Test/run_corpus.py) cannot see at all, since it only detects "didn't
resolve" or "crashed," never "resolved to the wrong thing."

Usage:
    python3 Test/round_trip.py <pkg>                  # single package
    python3 Test/round_trip.py --batch MANIFEST        # every CLEAN package
                                                        # in a Tier 1 manifest
    python3 Test/round_trip.py --batch MANIFEST --limit 100

Run from the repo root, same as Test/run_corpus.py and testdec.
"""

import argparse
import concurrent.futures
import difflib
import json
import re
import subprocess
import sys
import time
from pathlib import Path

DEFAULT_TIMEOUT = 20.0

REF_LABEL_RE = re.compile(r"\bRef_\d+\b")


def normalize_ref_labels(text: str) -> str:
    """Replace every `Ref_NNN` label with a canonical name numbered by
    first-appearance order. `Ref_N` numbering is an artifact of literal-
    table assignment order in our own compiler -- recompiling structurally
    identical source can still shift every label by a constant offset (one
    extra/fewer literal anywhere earlier in the table cascades forward),
    which is numbering noise, not a real behavioral difference. Comparing
    gen2 vs gen3 without this normalization produces false MISMATCHes that
    are 100% label renumbering and 0% actual content difference."""
    seen = {}
    def repl(m):
        label = m.group(0)
        if label not in seen:
            seen[label] = f"Ref_{len(seen):05d}"
        return seen[label]
    return REF_LABEL_RE.sub(repl, text)


SYNTAX_ERROR_MARKERS = ("!!! Exception:", "syntax error")


def decompile(newtc: str, args: list, timeout: float) -> str | None:
    """Run newtc with the given extra args + -decompile, return stdout+stderr
    merged, or None on crash/timeout/parse failure. A `-script` recompile of
    a prior generation's decompiled output can fail *without* a non-zero
    exit code -- newtc prints a "!!! Exception: ..., syntax error" message
    inline and carries on with a degenerate (usually `nil`) result -- so
    that has to be checked for explicitly, not inferred from returncode,
    or a gen1-output round-trip gap silently cascades into a meaningless
    gen2-vs-gen3 "mismatch" that's actually just gen2 already being broken."""
    try:
        proc = subprocess.run(
            [newtc, *args, "-decompile"],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            encoding="utf-8",
            errors="replace",
        )
    except subprocess.TimeoutExpired:
        return None
    if proc.returncode != 0:
        return None
    out = proc.stdout or ""
    if any(marker in out for marker in SYNTAX_ERROR_MARKERS):
        return None
    return out


def recompile_and_decompile(newtc: str, source_text: str, tmp_path: Path, timeout: float) -> str | None:
    tmp_path.write_text(source_text)
    return decompile(newtc, ["-script", str(tmp_path)], timeout)


def check_one(newtc: str, pkg_path: str, work_dir: Path, timeout: float) -> dict:
    """Returns a dict: status (OK/MISMATCH/GEN1_FAILED/GEN2_FAILED/GEN3_FAILED),
    plus gen1_vs_gen2 informational note and, on MISMATCH, a unified diff."""
    tag = str(abs(hash(pkg_path)))[:12]
    gen2_script = work_dir / f"{tag}_gen2.txt"
    gen3_script = work_dir / f"{tag}_gen3.txt"

    gen1_src = decompile(newtc, ["-pkg", pkg_path], timeout)
    if gen1_src is None:
        return {"pkg": pkg_path, "status": "GEN1_FAILED"}

    gen2_src = recompile_and_decompile(newtc, gen1_src, gen2_script, timeout)
    if gen2_src is None:
        return {"pkg": pkg_path, "status": "GEN2_FAILED"}

    gen3_src = recompile_and_decompile(newtc, gen2_src, gen3_script, timeout)
    if gen3_src is None:
        return {"pkg": pkg_path, "status": "GEN3_FAILED"}

    gen2_norm = normalize_ref_labels(gen2_src)
    gen3_norm = normalize_ref_labels(gen3_src)
    result = {
        "pkg": pkg_path,
        "gen1_eq_gen2": normalize_ref_labels(gen1_src) == gen2_norm,
    }
    if gen2_norm == gen3_norm:
        result["status"] = "OK"
    else:
        result["status"] = "MISMATCH"
        diff = list(difflib.unified_diff(
            gen2_norm.splitlines(), gen3_norm.splitlines(),
            fromfile="gen2", tofile="gen3", lineterm="", n=2))
        result["diff"] = diff[:200]
    gen2_script.unlink(missing_ok=True)
    gen3_script.unlink(missing_ok=True)
    return result


def load_clean_packages(manifest_path: Path):
    manifest = json.loads(manifest_path.read_text())
    return [p["pkg"] for p in manifest["packages"] if p["status"] == "CLEAN"]


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pkg", nargs="?", help="a single package to check")
    ap.add_argument("--batch", type=Path, metavar="MANIFEST",
                     help="run over every CLEAN package in a Test/run_corpus.py manifest.json")
    ap.add_argument("--newtc", default="build/VSCode/newtc")
    ap.add_argument("--timeout", default=DEFAULT_TIMEOUT, type=float)
    ap.add_argument("--jobs", default=None, type=int)
    ap.add_argument("--limit", default=None, type=int)
    ap.add_argument("--work-dir", default="/tmp", type=Path)
    args = ap.parse_args()

    newtc = str(Path(args.newtc).resolve())
    if not Path(newtc).exists():
        sys.exit(f"newtc binary not found at {newtc}")

    if not args.pkg and not args.batch:
        ap.error("provide a package path or --batch MANIFEST")

    if args.pkg:
        r = check_one(newtc, args.pkg, args.work_dir, args.timeout)
        print(json.dumps(r, indent=2) if r["status"] != "MISMATCH"
              else json.dumps({**r, "diff": "\n".join(r["diff"])}, indent=2))
        sys.exit(0 if r["status"] == "OK" else 1)

    packages = load_clean_packages(args.batch)
    if args.limit:
        packages = packages[:args.limit]
    print(f"Checking {len(packages)} CLEAN packages for gen2/gen3 fixed-point...", file=sys.stderr)

    results = []
    done = 0
    t0 = time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futures = [ex.submit(check_one, newtc, p, args.work_dir, args.timeout) for p in packages]
        for fut in concurrent.futures.as_completed(futures):
            results.append(fut.result())
            done += 1
            if done % 50 == 0 or done == len(packages):
                print(f"  {done}/{len(packages)} ({time.monotonic() - t0:.0f}s elapsed)", file=sys.stderr)

    by_status = {}
    for r in results:
        by_status.setdefault(r["status"], []).append(r)

    print(f"\nTotal: {len(results)}")
    for status, items in sorted(by_status.items()):
        print(f"  {status}: {len(items)}")
    gen1_diff_count = sum(1 for r in results if r.get("gen1_eq_gen2") is False)
    print(f"\n(informational) gen1 != gen2 (expected for NTK-optimized packages): {gen1_diff_count}")

    mismatches = by_status.get("MISMATCH", [])
    if mismatches:
        print(f"\n=== {len(mismatches)} gen2/gen3 MISMATCHES (real bugs, not optimization artifacts) ===")
        for r in mismatches[:20]:
            print(f"\n{r['pkg']}")
            print("\n".join(r["diff"][:30]))

    out = {"totals": {s: len(items) for s, items in by_status.items()}, "results": results}
    out_path = Path("Test/corpus_results/latest_roundtrip.json")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(out, indent=2))
    print(f"\nFull results: {out_path}", file=sys.stderr)


if __name__ == "__main__":
    main()
