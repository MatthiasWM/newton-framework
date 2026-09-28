#!/usr/bin/env python3
"""The internal store in a file (newtc -store; Stores/HostStore.h): what a
program puts in a soup is there in the next run, a store that isn't one is
replaced by a new one, and without -store every run starts empty. A run
keeps the file as it found it as <file>.bak; a file that isn't whole (cut
short, a changed byte: its CRC) is kept aside as <file>.bad-<time>, and
the backup is used. Version 1 files (no CRC) are read.

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
        backup = Path(str(store) + ".bak")
        checks.append(("a run keeps the file it found as .bak, the one before as .bak2",
                       backup.exists() and Path(str(store) + ".bak2").exists()))

        def bad_files():
            return sorted(p for p in tmp.iterdir() if ".bad-" in p.name)

        # the backup has the count of the run before the last (2); cut the file short
        whole = store.read_bytes()
        store.write_bytes(whole[:len(whole) - 10])
        out, err = run("-store", str(store))
        checks.append(("a store cut short: kept aside, the backup used",
                       out[0] == "3" and "kept as" in err and "using the backup" in err and len(bad_files()) == 1))
        # a changed byte: the CRC
        whole = bytearray(store.read_bytes())
        whole[len(whole) // 2] ^= 0x40
        store.write_bytes(bytes(whole))
        out, err = run("-store", str(store))
        checks.append(("a changed byte (CRC): the backup used", "using the backup" in err and out[0] != "1"))
        # version 1 (no CRC): read
        count = int(run("-store", str(store))[0][0])
        whole = bytearray(store.read_bytes())[:-4]
        whole[8:12] = (1).to_bytes(4, "little")
        store.write_bytes(bytes(whole))
        out, err = run("-store", str(store))
        checks.append(("a version 1 file is read", out[0] == str(count + 1) and err == ""))
        # neither the file nor the backup: a new store
        for p in bad_files():
            p.unlink()
        store.write_bytes(b"not a store")
        backup.write_bytes(b"no backup either")
        out, err = run("-store", str(store))
        checks.append(("a file that isn't a store, no backup: a new one", out[0] == "1" and "starting a new store" in err))
        checks.append(("... and it is a store now", run("-store", str(store))[0][0] == "2"))
    failed = 0
    for name, ok in checks:
        print(("ok      " if ok else "FAIL    ") + name)
        failed += not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
