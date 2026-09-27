#!/usr/bin/env python3
"""The internal store in a file (newtc -store; Stores/HostStore.h): what a
program puts in a soup is there in the next run, a store that isn't one is
replaced by a new one, and without -store every run starts empty.

    test_store.py [--newtc path]
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

COUNT = r'''
DefGlobalVar('so, GetStores()[0]:GetSoup("System"));
DefGlobalVar('c, Query(so, {type: 'index, indexPath: 'tag, startKey: "newtc runs",
  endTest: func(x) not StrEqual(x.tag, "newtc runs")}));
DefGlobalVar('e, c:Entry());
if e then begin e.runs := e.runs + 1; EntryChange(e) end
else e := so:Add({tag: "newtc runs", runs: 1});
Print(e.runs);
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--newtc", default=str(ROOT / "build" / "VSCode" / "newtc"))
    args = ap.parse_args()
    checks = []
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        script = tmp / "count.ns"
        script.write_text(COUNT)
        store = tmp / "internal.store"

        def run(*extra):
            p = subprocess.run([args.newtc, *extra, "-script", str(script)],
                               capture_output=True, text=True, timeout=30)
            return p.stdout.strip().splitlines()[-1:] or [""], p.stderr

        runs = [run("-store", str(store))[0][0] for _ in range(3)]
        checks.append(("the soup entry is there in the next runs", runs == ["1", "2", "3"]))
        checks.append(("the store file exists", store.exists()))
        checks.append(("without -store, a new store", run()[0][0] == "1"))
        store.write_bytes(b"not a store")
        out, err = run("-store", str(store))
        checks.append(("a file that isn't a store: a new one", out[0] == "1" and "not a newtc store" in err))
        checks.append(("... and it is a store now", run("-store", str(store))[0][0] == "2"))
    failed = 0
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
        failed += not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
