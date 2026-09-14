#!/usr/bin/env python3
"""
Test/run_corpus.py -- Tier 1 corpus-wide sweep for Matt's NewtonScript decompiler.

Runs `newtc -pkg <file> -decompile` over every candidate package listed in
a pkglist-style file (default: Test/pkglist.txt), each as an *isolated
subprocess* -- never via newtc's own `-pkglist`, whose internal loop does
not survive a crash mid-batch (only package *loading* is wrapped in
try/catch there, not the -decompile call itself). Classifies each result
as CLEAN / UNRESOLVED / CRASHED / TIMEOUT, and groups UNRESOLVED/CRASHED
results by a failure *fingerprint* (the sequence of raw unresolved node
classes, or the crash signal + stderr tail) so that many packages sharing
one root cause show up as a single cluster instead of N unrelated
failures -- see Matt/CLAUDE.md's `viewSetupFormScript`/argFrame finding
for a real example of this recurring across unrelated packages.

Run from the repo root (matches every other tool/recipe in this project):

    python3 Test/run_corpus.py
    python3 Test/run_corpus.py --limit 100          # quick smoke test
    python3 Test/run_corpus.py --all-categories      # include books/sounds/fonts/movies
    python3 Test/run_corpus.py --compare OLD_MANIFEST NEW_MANIFEST

See Matt/CLAUDE.md, "Corpus-scale testing" for the full workflow this is
part of.
"""

import argparse
import concurrent.futures
import dataclasses
import hashlib
import json
import re
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path

# First-pass scope decision (see the plan this was built from): books,
# sounds, fonts and movies are real categories in the corpus but low value
# for finding control-flow/decompilation bugs -- excluded by default to
# keep iteration fast, not permanently out of scope. Override with
# --all-categories or --categories.
DEFAULT_EXCLUDED_CATEGORIES = {"books", "sounds", "fonts", "movies"}

# Matches Decompiler::decompile()'s diagnostic block (Decompiler.cc, near
# the end of the function):
#   WARNING: %d unresolved nodes in AST.
#   Path: '%s'
#   PC: <space-separated pcs>
# emitted to stderr, once per function that has unresolved nodes; combined
# with the "##### [P:x] pc=y: ClassName ...;" raw-node dump lines that
# Node::PrintNode() emits to stdout for each unresolved node's own position
# in the function body, immediately before the WARNING for that function.
WARNING_RE = re.compile(r"^WARNING: (\d+) unresolved nodes in AST\.$")
PATH_RE = re.compile(r"^Path: '(.*)'$")
PC_RE = re.compile(r"^PC:(.*)$")
NODE_RE = re.compile(r"#####\s*\[P:\s*-?\d+\]\s*pc=\s*(\d+):\s*(\w+)")


@dataclasses.dataclass
class PackageResult:
    pkg_path: str
    category: str
    status: str  # CLEAN | UNRESOLVED | CRASHED | TIMEOUT
    elapsed: float
    unresolved: list = dataclasses.field(default_factory=list)  # list of dicts: path/count/fingerprint
    crash_signature: str | None = None
    log_text: str | None = None  # only populated for non-CLEAN results


def category_of(pkg_path: str) -> str:
    marker = "/unna2/"
    idx = pkg_path.find(marker)
    if idx == -1:
        return "other"
    rest = pkg_path[idx + len(marker):]
    parts = rest.split("/", 1)
    return parts[0] if parts else "other"


def load_candidates(pkglist_path: Path):
    candidates = []
    for line in pkglist_path.read_text(errors="replace").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        candidates.append(line)
    return candidates


def parse_unresolved_blocks(text: str):
    """Parse merged stdout+stderr into a list of {path, count, fingerprint}
    dicts, one per WARNING block. `fingerprint` is the comma-joined list of
    raw node class names, in the same order as the WARNING's own PC list --
    an approximate but effective clustering key: the same underlying bug
    tends to leave the exact same shape of unresolved nodes behind,
    verbatim, across unrelated packages built from shared library code."""
    blocks = []
    pc_to_class = {}
    pending_count = None
    pending_path = None
    for line in text.splitlines():
        m = NODE_RE.search(line)
        if m:
            pc_to_class[int(m.group(1))] = m.group(2)
            continue
        m = WARNING_RE.match(line)
        if m:
            pending_count = int(m.group(1))
            continue
        m = PATH_RE.match(line)
        if m and pending_count is not None:
            pending_path = m.group(1)
            continue
        m = PC_RE.match(line)
        if m and pending_count is not None:
            pcs = [int(x) for x in m.group(1).split()]
            classes = [pc_to_class.get(pc, "?") for pc in pcs]
            blocks.append({
                "path": pending_path,
                "count": pending_count,
                "fingerprint": ",".join(classes),
            })
            pending_count = None
            pending_path = None
            pc_to_class = {}
    return blocks


def run_one(newtc: str, pkg_path: str, timeout: float) -> PackageResult:
    category = category_of(pkg_path)
    start = time.monotonic()
    try:
        proc = subprocess.run(
            [newtc, "-pkg", pkg_path, "-decompile"],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            encoding="utf-8",
            errors="replace",
        )
    except subprocess.TimeoutExpired:
        elapsed = time.monotonic() - start
        return PackageResult(pkg_path, category, "TIMEOUT", elapsed,
                              crash_signature=f"timeout after {timeout}s")
    elapsed = time.monotonic() - start
    out = proc.stdout or ""

    if proc.returncode != 0:
        tail = "\n".join(out.splitlines()[-12:])
        sig = f"exit={proc.returncode}: {tail.strip()[:300]}"
        return PackageResult(pkg_path, category, "CRASHED", elapsed,
                              crash_signature=sig, log_text=out)

    blocks = parse_unresolved_blocks(out)
    if blocks:
        return PackageResult(pkg_path, category, "UNRESOLVED", elapsed,
                              unresolved=blocks, log_text=out)
    return PackageResult(pkg_path, category, "CLEAN", elapsed)


def log_filename(pkg_path: str) -> str:
    h = hashlib.sha1(pkg_path.encode("utf-8", "replace")).hexdigest()[:16]
    return f"{h}.log"


def build_manifest(results, meta):
    by_status = defaultdict(int)
    by_category_status = defaultdict(lambda: defaultdict(int))
    unresolved_clusters = defaultdict(list)
    crash_clusters = defaultdict(list)
    packages = []

    for r in results:
        by_status[r.status] += 1
        by_category_status[r.category][r.status] += 1
        entry = {
            "pkg": r.pkg_path,
            "category": r.category,
            "status": r.status,
            "elapsed": round(r.elapsed, 3),
        }
        if r.status == "UNRESOLVED":
            entry["unresolved"] = r.unresolved
            for b in r.unresolved:
                unresolved_clusters[b["fingerprint"]].append(
                    {"pkg": r.pkg_path, "path": b["path"], "count": b["count"]})
        if r.status in ("CRASHED", "TIMEOUT"):
            entry["crash_signature"] = r.crash_signature
            crash_clusters[r.crash_signature].append(r.pkg_path)
        if r.log_text is not None:
            entry["log"] = f"logs/{log_filename(r.pkg_path)}"
        packages.append(entry)

    return {
        "meta": meta,
        "totals": dict(by_status),
        "by_category": {c: dict(s) for c, s in by_category_status.items()},
        "unresolved_clusters": [
            {"fingerprint": fp, "count": len(items), "examples": items[:10]}
            for fp, items in sorted(unresolved_clusters.items(), key=lambda kv: -len(kv[1]))
        ],
        "crash_clusters": [
            {"signature": sig, "count": len(items), "examples": items[:10]}
            for sig, items in sorted(crash_clusters.items(), key=lambda kv: -len(kv[1]))
        ],
        "packages": packages,
    }


def format_summary(manifest) -> str:
    lines = []
    t = manifest["totals"]
    total = sum(t.values())
    lines.append(f"Total packages: {total}")
    for status in ("CLEAN", "UNRESOLVED", "CRASHED", "TIMEOUT"):
        if status in t:
            lines.append(f"  {status}: {t[status]}")
    lines.append("\nBy category:")
    for cat, statuses in sorted(manifest["by_category"].items()):
        parts = ", ".join(f"{s}={n}" for s, n in sorted(statuses.items()))
        lines.append(f"  {cat}: {parts}")
    lines.append(f"\nTop unresolved-fingerprint clusters ({len(manifest['unresolved_clusters'])} distinct):")
    for c in manifest["unresolved_clusters"][:20]:
        lines.append(f"  [{c['count']}x] {c['fingerprint'][:120]}")
        for ex in c["examples"][:3]:
            lines.append(f"      e.g. {ex['pkg']} :: {ex['path']}")
    lines.append(f"\nTop crash-signature clusters ({len(manifest['crash_clusters'])} distinct):")
    for c in manifest["crash_clusters"][:20]:
        lines.append(f"  [{c['count']}x] {c['signature'][:150]}")
        for ex in c["examples"][:3]:
            lines.append(f"      e.g. {ex}")
    return "\n".join(lines)


def compare_manifests(old_path: Path, new_path: Path):
    old = json.loads(old_path.read_text())
    new = json.loads(new_path.read_text())
    old_status = {p["pkg"]: p["status"] for p in old["packages"]}
    new_status = {p["pkg"]: p["status"] for p in new["packages"]}
    all_pkgs = sorted(set(old_status) | set(new_status))

    fixed, regressed, changed = [], [], []
    for pkg in all_pkgs:
        o = old_status.get(pkg, "MISSING")
        n = new_status.get(pkg, "MISSING")
        if o == n:
            continue
        if n == "CLEAN" and o != "CLEAN":
            fixed.append((pkg, o, n))
        elif o == "CLEAN" and n != "CLEAN":
            regressed.append((pkg, o, n))
        else:
            changed.append((pkg, o, n))

    print(f"Old totals: {old['totals']}")
    print(f"New totals: {new['totals']}")
    print(f"\nFixed ({len(fixed)}):")
    for pkg, o, n in fixed[:50]:
        print(f"  {o} -> {n}: {pkg}")
    print(f"\nREGRESSED ({len(regressed)}):")
    for pkg, o, n in regressed[:50]:
        print(f"  {o} -> {n}: {pkg}")
    if changed:
        print(f"\nOther status changes ({len(changed)}):")
        for pkg, o, n in changed[:50]:
            print(f"  {o} -> {n}: {pkg}")

    print(f"\nUnresolved clusters: {len(old.get('unresolved_clusters', []))} -> {len(new.get('unresolved_clusters', []))}")
    print(f"Crash clusters:      {len(old.get('crash_clusters', []))} -> {len(new.get('crash_clusters', []))}")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pkglist", default="Test/pkglist.txt", type=Path)
    ap.add_argument("--newtc", default="build/VSCode/newtc")
    ap.add_argument("--out", default=None, type=Path,
                     help="results directory (default: Test/corpus_results/<timestamp>)")
    ap.add_argument("--timeout", default=20.0, type=float,
                     help="per-package subprocess timeout in seconds (default: 20)")
    ap.add_argument("--jobs", default=None, type=int,
                     help="parallel workers (default: os.cpu_count())")
    ap.add_argument("--limit", default=None, type=int,
                     help="only run the first N candidates (for quick smoke tests)")
    ap.add_argument("--categories", default=None,
                     help="comma-separated list of categories to INCLUDE "
                          "(overrides the default exclusion list entirely)")
    ap.add_argument("--all-categories", action="store_true",
                     help="include every category, including books/sounds/fonts/movies")
    ap.add_argument("--compare", nargs=2, metavar=("OLD_MANIFEST", "NEW_MANIFEST"),
                     help="compare two prior manifest.json files instead of running a sweep")
    args = ap.parse_args()

    if args.compare:
        compare_manifests(Path(args.compare[0]), Path(args.compare[1]))
        return

    newtc = str(Path(args.newtc).resolve())
    if not Path(newtc).exists():
        sys.exit(f"newtc binary not found at {newtc} -- build it first "
                  f"(cmake --build build/VSCode --target newtc)")

    all_candidates = load_candidates(args.pkglist)

    if args.categories:
        include = {c.strip() for c in args.categories.split(",")}
        candidates = [p for p in all_candidates if category_of(p) in include]
    elif args.all_categories:
        candidates = all_candidates
    else:
        candidates = [p for p in all_candidates if category_of(p) not in DEFAULT_EXCLUDED_CATEGORIES]

    if args.limit:
        candidates = candidates[:args.limit]

    print(f"Candidates: {len(candidates)} (of {len(all_candidates)} total in {args.pkglist})",
          file=sys.stderr)

    timestamp = time.strftime("%Y%m%d-%H%M%S")
    out_dir = args.out or Path("Test/corpus_results") / timestamp
    logs_dir = out_dir / "logs"
    logs_dir.mkdir(parents=True, exist_ok=True)

    results = []
    done = 0
    t0 = time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futures = [ex.submit(run_one, newtc, p, args.timeout) for p in candidates]
        for fut in concurrent.futures.as_completed(futures):
            r = fut.result()
            results.append(r)
            if r.log_text is not None:
                (logs_dir / log_filename(r.pkg_path)).write_text(
                    f"# {r.pkg_path}\n# status={r.status}\n\n{r.log_text}", errors="replace")
            done += 1
            if done % 50 == 0 or done == len(candidates):
                print(f"  {done}/{len(candidates)} ({time.monotonic() - t0:.0f}s elapsed)",
                      file=sys.stderr)

    meta = {
        "timestamp": timestamp,
        "pkglist": str(args.pkglist),
        "newtc": newtc,
        "candidate_count": len(candidates),
        "total_candidate_count": len(all_candidates),
        "timeout": args.timeout,
        "elapsed_seconds": round(time.monotonic() - t0, 1),
    }
    manifest = build_manifest(results, meta)

    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2))
    summary = format_summary(manifest)
    (out_dir / "summary.txt").write_text(summary)

    # Stable, un-timestamped copies for CLAUDE.md/scripts to point at.
    latest_dir = Path("Test/corpus_results")
    (latest_dir / "latest_manifest.json").write_text(json.dumps(manifest, indent=2))
    (latest_dir / "latest_summary.txt").write_text(summary)

    print(file=sys.stderr)
    print(summary)
    print(f"\nFull results: {out_dir}/", file=sys.stderr)


if __name__ == "__main__":
    main()
