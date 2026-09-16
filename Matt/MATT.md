

# 1. Rebuild
cmake --build build/VSCode --target newtc -j4

# 2. Quick regression check (12-package sample) — catches obvious breaks fast,
#    before spending time on the full corpus sweep
mkdir -p /tmp/regress_check
i=0; while read -r pkg; do
  [ -z "$pkg" ] && continue
  case "$pkg" in \#*) continue;; esac
  i=$((i+1))
  ./build/VSCode/newtc -pkg "$pkg" -decompile > "/tmp/regress_check/$i.out" 2>&1 </dev/null
done < /tmp/regress_list.txt
for i in $(seq 1 12); do
  diff -q "/tmp/regress_foreach_combined_fix/$i.out" "/tmp/regress_check/$i.out" > /dev/null 2>&1 || echo "DIFFERS: file $i"
done

# 3. Preserve the current manifest as "before", then run the full corpus sweep
#    (this overwrites Test/corpus_results/latest_manifest.json and
#    latest_summary.txt, and also writes a fresh timestamped copy under
#    Test/corpus_results/<timestamp>/)
cp Test/corpus_results/latest_manifest.json /tmp/manifest_before_printdependents_fix.json
python3 Test/run_corpus.py --jobs 16 --timeout 20

# 4. Compare old vs new — this is the actual regression check
python3 Test/run_corpus.py --compare /tmp/manifest_before_printdependents_fix.json Test/corpus_results/latest_manifest.json

# 5. (optional but recommended) Tier 2 self-consistency spot-check
python3 Test/round_trip.py --batch Test/corpus_results/latest_manifest.json --limit 200 --jobs 12


### REPL style Debugger ###

You have two source situations, and both point the same way: for packages you extract/decompile from the original ROM, you can't (and shouldn't) touch the package binary — it needs to stay byte-identical to the original for compatibility testing. For packages you compile yourself with newtc, you could embed a debug segment, but keeping the format identical either way means one code path in the debugger instead of two, and it keeps release builds trivially strippable (just don't emit the side-car). So: Foo.pkg + Foo.nsdbg next to it, same base name.

Two robustness details worth building in from day one since these files will drift apart over time as you re-decompile things:

Store a checksum of the package's code segment inside the .nsdbg file, and have the debugger refuse (or warn loudly) if it doesn't match the loaded package — cheap insurance against silently misattributing lines after a re-decompile.
Key by that checksum rather than (or in addition to) filename if you ever expect packages to get renamed/moved independently of their debug info — basically the debuginfod/build-id idea, though for a personal tool, filename pairing is probably enough to start.

Format: don't reach for DWARF or JS-style source maps — you need much less than either. Both are designed for interop with a broad tooling ecosystem you don't need to interoperate with; a minimal custom binary table will be smaller and simpler to write/read than shoehorning your data into either.

The standard trick (same idea as DWARF's line program, Python's co_linetable) is a delta-encoded, PC-ordered table, because within a statement many consecutive bytecodes share the same line:

entry := pc_delta: uleb128, line_delta: zigzag-leb128, file_delta: uleb128 (0 = same file)

Store the sequence sorted by PC, plus a small trailing string table for filenames referenced by index. To resolve PC → (file, line) at debug time, decode once into a flat sorted array in memory and binary-search it — at package sizes up to 4MB you're talking a few thousand instructions at most, so the whole table is tens of KB uncompressed and decode is instant. No need for anything fancier (no column info, no is-statement/basic-block flags like DWARF carries — you don't need those for a source-line stepping debugger).

The nice part: you already have the infrastructure for this from the compiler-diagnostics discussion — your codegen already knows the current source position at every emit call. Just append a table entry whenever that position differs from the last one you recorded; it falls out of the existing pipeline almost for free and is naturally run-length-friendly (most instructions produce a zero line-delta).


Our package or streaming object can be compiled from multiple files. So we need
map the file and line number to a function and pc when storing debug data. We need
to map the function and pc to a memory address, so we can set breakpoints.

During debugging, we also need to map the memory address to a file and line
number, so we can display the source code.

PCs are offsets to the start of binary data, so all we need to store is the start
and size of a bytecode block. We can then use the pc to index into the
binary data and find the correct line.

../../newtonresearch/newton-toolkit/NTX/Context/Engineering/platform.Dump.ns

GloballyEnableBreakpoints() -> use "slow" interpreter
BreakLoop()
BreakOnThrows()
STackTrace()
Where()
GetCurrentPC()
GetCurrentFunction()
point := InstallBreakPoint(Debug("mySlider").changedSlider,0);
RemoveBreakPoint();
RemoveAllBreakPoints();
EnableBreakPoint();
SetBreakPointLabel();
GloballyEnableBreakPoints();
NSDBreakLoopEntry(), NSDBreakLoopExit -> callback for conditional breakpoints

StackTrace, GetCurrentFunction,
GetCurrentPC, SetCurrentPC, Where, GetAllTempVars,
GetTempVar, SetTempVar, GetAllNamedVars, GetNamedVar,
SetNamedVar, GetCurrentReceiver, and GetCurrentImplementor
GetPathToSlot and GetPathWhereSet
Disasm

Step, StepIn, StepOut, RunUntil

The stepping functions create temporary break points, which are removed as
soon as they’re used.

gFramesBreakPoints

SetBreakPoints(Ref bps)
{ programCounter: [
    {
      programCounter: 1000;
      instructions: ; -> ref to binary data block
      disabled: true/NIL;
      temporary: true/NIL;
    }
  ]
}
CInterpreter::handleBreakPoints() is never called

