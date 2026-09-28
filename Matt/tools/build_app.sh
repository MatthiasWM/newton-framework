#!/bin/sh
# Build a Newton package as a macOS app of its own (newtc with the package
# compiled in; Matt/EmbeddedApp.h): a Release build with FLTK, without
# sanitizers, for macOS 13 (Ventura) and later, universal (Apple Silicon
# and Intel).
#
#   Matt/tools/build_app.sh <app.pkg> [name] [version]
#
# Writes build/App/<name>.app and a zip of it next to it (to send).
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
PKG=${1:?"usage: build_app.sh <app.pkg> [name] [version]"}
NAME=${2:-$(basename "$PKG" .pkg)}
VERSION=${3:-1.0}
BUILD="$REPO/build/App"

cmake -S "$REPO" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DNEWTC_UBSAN=OFF \
  -DNEWTC_USES_FLTK=ON \
  -DNEWTC_APP_PKG="$PKG" \
  -DNEWTC_APP_NAME="$NAME" \
  -DNEWTC_APP_VERSION="$VERSION"
cmake --build "$BUILD" --target newtc_app

APP="$BUILD/$NAME.app"
# both architectures, only system libraries
for ARCH in arm64 x86_64; do
  if otool -arch $ARCH -L "$APP/Contents/MacOS/$NAME" | tail -n +2 | grep -v -e '^\s*/usr/lib/' -e '^\s*/System/Library/'; then
    echo "error: the app ($ARCH) links a library that isn't part of macOS (see above)" >&2
    exit 1
  fi
done
codesign --verify --deep "$APP"
(cd "$BUILD" && rm -f "$NAME.zip" && ditto -c -k --keepParent "$NAME.app" "$NAME.zip")
echo "built $APP ($(du -sh "$APP" | cut -f1)) and $BUILD/$NAME.zip"
