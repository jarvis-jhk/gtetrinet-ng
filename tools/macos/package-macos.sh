#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# The whole macOS release path, shared by CI and the release workflow: universal Release
# build against the pinned SDL2, CTest, the .app, start checks of both architectures from
# inside the bundle, and the zip a player downloads.
# Usage: package-macos.sh SDL_PREFIX VERSION FAULT_REPORTS(ON|OFF) OUT_DIR
set -euo pipefail

SDL=$1 VERSION=$2 FAULTS=$3 OUT=$4
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build-macos"
APP="$OUT/GTetrinet NG.app"
export MACOSX_DEPLOYMENT_TARGET=11.0 GTN_NO_REPORT=1

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$SDL" \
  -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' -DGTN_VERSION="$VERSION" -DGTN_FAULT_REPORTS="$FAULTS"
cmake --build "$BUILD" --parallel 3
ctest --test-dir "$BUILD" --output-on-failure

mkdir -p "$OUT"
art=$(mktemp -d)
SDL_VIDEODRIVER=dummy "$BUILD/gtetrinet-ng" --window --demo 8 --frames 120 --art "$art"
sips -s format png "$art/steam_icon.bmp" --out "$art/icon.png" >/dev/null
bash "$ROOT/tools/macos/bundle-macos.sh" "$BUILD" "$SDL" "$APP" "$VERSION" "$art/icon.png"

# Started from inside the bundle, so only @rpath and the bundled SDL can satisfy the loader.
# The x86_64 slice runs under Rosetta on an Apple Silicon runner.
bin="$APP/Contents/MacOS/gtetrinet-ng"
"$bin" --version | grep -qx "gtetrinet-ng $VERSION"
DYLD_PRINT_LIBRARIES=1 SDL_VIDEODRIVER=dummy "$bin" --window --demo 8 --frames 600 --screenshot "$OUT/smoke-arm64.bmp" 2> "$OUT/dyld.log"
grep libSDL2 "$OUT/dyld.log"
grep -q "Contents/MacOS/../Frameworks/libSDL2-2.0.0.dylib\|Contents/Frameworks/libSDL2-2.0.0.dylib" "$OUT/dyld.log"
test -s "$OUT/smoke-arm64.bmp"
if arch -x86_64 /usr/bin/true 2>/dev/null; then
  arch -x86_64 "$bin" --version | grep -qx "gtetrinet-ng $VERSION"
  SDL_VIDEODRIVER=dummy arch -x86_64 "$bin" --window --demo 8 --frames 600 --screenshot "$OUT/smoke-x86_64.bmp"
  test -s "$OUT/smoke-x86_64.bmp"
else
  echo "package-macos: no Rosetta on this runner, x86_64 slice not started" >&2
  exit 1
fi
# A real Cocoa window and the Metal/OpenGL renderer, as a player gets it.
"$bin" --window --demo 8 --frames 300 --screenshot "$OUT/smoke-cocoa.bmp"
test -s "$OUT/smoke-cocoa.bmp"
server="$APP/Contents/MacOS/gtetrinet-ng-server"
"$server" --port 31457 --local --quiet &
pid=$!
sleep 1
"$server" --query 127.0.0.1:31457
kill "$pid"

(cd "$OUT" && ditto -c -k --sequesterRsrc --keepParent "GTetrinet NG.app" gtetrinet-ng-macos-universal.zip)
# The download must still verify after a round trip through the zip.
check=$(mktemp -d)
ditto -x -k "$OUT/gtetrinet-ng-macos-universal.zip" "$check"
codesign --verify --deep --strict "$check/GTetrinet NG.app"
"$check/GTetrinet NG.app/Contents/MacOS/gtetrinet-ng" --version
