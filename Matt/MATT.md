

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
