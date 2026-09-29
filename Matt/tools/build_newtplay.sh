#!/bin/sh
# Build NewtPlay.app (Matt/NewtPlay.h): a Release build with FLTK, without
# sanitizers, for macOS 13 (Ventura) and later, universal (Apple Silicon
# and Intel).
#
#   Matt/tools/build_newtplay.sh [version]
#
# Writes build/NewtPlay/NewtPlay.app and NewtPlay.zip next to it. Signed ad
# hoc, unless (for other Macs, without a Gatekeeper warning):
#   SIGN_IDENTITY="Developer ID Application: Name (TEAMID)"  signs it with
#       that certificate (from the keychain), with the hardened runtime;
#   NOTARY_PROFILE=<profile>  also has Apple notarize it (xcrun notarytool,
#       credentials stored with notarytool store-credentials <profile>) and
#       staples the ticket to it.
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
VERSION=${1:-0.1}
BUILD="$REPO/build/NewtPlay"

cmake -S "$REPO" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DNEWTC_UBSAN=OFF \
  -DNEWTC_USES_FLTK=ON \
  -DNEWTPLAY_VERSION="$VERSION"
cmake --build "$BUILD" --target NewtPlay

APP="$BUILD/NewtPlay.app"
# both architectures, only system libraries
for ARCH in arm64 x86_64; do
  if otool -arch $ARCH -L "$APP/Contents/MacOS/NewtPlay" | tail -n +2 | grep -v -e '^\s*/usr/lib/' -e '^\s*/System/Library/'; then
    echo "error: NewtPlay ($ARCH) links a library that isn't part of macOS (see above)" >&2
    exit 1
  fi
done

ZIP="$BUILD/NewtPlay.zip"
zip_app() { (cd "$BUILD" && rm -f NewtPlay.zip && ditto -c -k --keepParent NewtPlay.app NewtPlay.zip); }

if [ -n "$SIGN_IDENTITY" ]; then
  codesign --force --options runtime --timestamp --sign "$SIGN_IDENTITY" "$APP"
  echo "signed with $SIGN_IDENTITY"
fi
codesign --verify --deep --strict "$APP"
zip_app
if [ -n "$NOTARY_PROFILE" ]; then
  [ -n "$SIGN_IDENTITY" ] || { echo "error: NOTARY_PROFILE needs SIGN_IDENTITY" >&2; exit 1; }
  xcrun notarytool submit "$ZIP" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$APP"
  zip_app   # again, with the ticket
  spctl --assess --type execute --verbose "$APP"
fi
echo "built $APP ($(du -sh "$APP" | cut -f1)) and $ZIP"
