#!/bin/sh
# Build the newtc that VSNewt bundles (macOS arm64) and copy it into the
# extension: a Release build without sanitizers (their runtime libraries
# come from the toolchain and aren't on other Macs), for macOS 13 (Ventura)
# and later.
#
#   Matt/tools/build_vsnewt_newtc.sh [path/to/vsnewt]
#
# Default extension folder: ../VSNewt.git/vsnewt next to this repository.
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
VSNEWT=${1:-"$REPO/../VSNewt.git/vsnewt"}
BUILD="$REPO/build/VSNewt"

cmake -S "$REPO" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DNEWTC_UBSAN=OFF
cmake --build "$BUILD" --target newtc

NEWTC="$BUILD/newtc"
# only system libraries, and the right minimum macOS
if otool -L "$NEWTC" | tail -n +2 | grep -v -e '^\s*/usr/lib/' -e '^\s*/System/Library/'; then
  echo "error: newtc links a library that isn't part of macOS (see above)" >&2
  exit 1
fi
vtool -show-build "$NEWTC" | grep -q 'minos 13.0' || { echo "error: newtc isn't built for macOS 13" >&2; exit 1; }
strip -x "$NEWTC"
codesign --force --sign - "$NEWTC"

mkdir -p "$VSNEWT/bin/darwin-arm64"
cp "$NEWTC" "$VSNEWT/bin/darwin-arm64/newtc"
chmod +x "$VSNEWT/bin/darwin-arm64/newtc"
echo "copied $(du -h "$NEWTC" | cut -f1) newtc ($(git -C "$REPO" rev-parse --short HEAD)) to $VSNEWT/bin/darwin-arm64/"
