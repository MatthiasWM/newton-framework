#!/bin/sh
# Build a Newton package as a macOS app of its own (newtc with the package
# compiled in; Matt/EmbeddedApp.h): a Release build with FLTK, without
# sanitizers, for macOS 13 (Ventura) and later, universal (Apple Silicon
# and Intel).
#
#   Matt/tools/build_app.sh <app.pkg> [name] [version]
#
# Writes build/App/<name>.app and a zip of it next to it (to send). Signed
# ad hoc, unless (for other Macs, without a Gatekeeper warning):
#   SIGN_IDENTITY="Developer ID Application: Name (TEAMID)"  signs it with
#       that certificate (from the keychain), with the hardened runtime;
#   NOTARY_PROFILE=<profile>  also has Apple notarize it (xcrun notarytool,
#       credentials stored with notarytool store-credentials <profile>) and
#       staples the ticket to it.
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
ZIP="$BUILD/$NAME.zip"
zip_app() { (cd "$BUILD" && rm -f "$NAME.zip" && ditto -c -k --keepParent "$NAME.app" "$NAME.zip"); }

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
