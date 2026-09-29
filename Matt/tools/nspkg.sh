#!/bin/sh
# Copy Newton packages as .nspkg, without quarantine, to try them with
# NewtPlay in the Finder. A downloaded .pkg is quarantined, and the Finder
# has Gatekeeper check a quarantined .pkg as an installer package before
# any app gets it (NewtPlay never sees it); a .nspkg is NewtPlay's own type,
# which Gatekeeper doesn't check.
#
#   Matt/tools/nspkg.sh <package or folder>... <destination folder>
#
# Folders are searched for *.pkg; files that aren't Newton packages (they
# start with "package0" or "package1") are left out.
set -e
[ $# -ge 2 ] || { echo "usage: nspkg.sh <package or folder>... <destination folder>" >&2; exit 1; }
for last; do :; done
DEST="$last"
mkdir -p "$DEST"
copied=0
while [ $# -gt 1 ]; do
  find "$1" -type f \( -iname '*.pkg' -o -iname '*.newtonpkg' -o -iname '*.nspkg' \) -print | while IFS= read -r f; do
    sig=$(head -c 7 "$f" 2>/dev/null)
    [ "$sig" = "package" ] || { echo "not a Newton package: $f" >&2; continue; }
    name=$(basename "$f"); name="${name%.*}"
    out="$DEST/$name.nspkg"
    cp "$f" "$out"
    xattr -c "$out"
    echo "$out"
  done
  shift
done
