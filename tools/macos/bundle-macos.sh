#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Turns a macOS build into "GTetrinet NG.app": the game and server in Contents/MacOS, SDL2 in
# Contents/Frameworks found through @rpath, CHANGELOG.md in Contents/Resources (where
# SDL_GetBasePath points inside a bundle, so the in-game news still finds it), ad-hoc signed.
# Usage: bundle-macos.sh BUILD_DIR SDL_PREFIX OUT.app VERSION [ICON.png]
set -euo pipefail

BUILD=$1 SDL=$2 APP=$3 VERSION=$4 ICON=${5:-}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SDL_LIB=libSDL2-2.0.0.dylib

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
cp "$BUILD/gtetrinet-ng" "$BUILD/gtetrinet-ng-server" "$APP/Contents/MacOS/"
cp -L "$SDL/lib/$SDL_LIB" "$APP/Contents/Frameworks/$SDL_LIB"
chmod u+w "$APP/Contents/Frameworks/$SDL_LIB"
install_name_tool -id "@rpath/$SDL_LIB" "$APP/Contents/Frameworks/$SDL_LIB"

game="$APP/Contents/MacOS/gtetrinet-ng"
# Point the game at the bundled copy and forget the build machine's library paths.
otool -L "$game" | awk 'NR > 1 && /libSDL2/ {print $1}' | sort -u | while read -r old; do
  install_name_tool -change "$old" "@rpath/$SDL_LIB" "$game"
done
# otool lists every slice of a universal binary; one delete covers both, so each path once.
otool -l "$game" | awk '/cmd LC_RPATH/ {getline; getline; print $2}' | sort -u | while read -r rp; do
  install_name_tool -delete_rpath "$rp" "$game"
done
install_name_tool -add_rpath "@executable_path/../Frameworks" "$game"

icon_key=""
if [ -n "$ICON" ]; then
  set_dir=$(mktemp -d)/gtetrinet-ng.iconset
  mkdir -p "$set_dir"
  for s in 16 32 128 256; do
    sips -z $s $s "$ICON" --out "$set_dir/icon_${s}x${s}.png" >/dev/null
    sips -z $((s * 2)) $((s * 2)) "$ICON" --out "$set_dir/icon_${s}x${s}@2x.png" >/dev/null
  done
  iconutil -c icns "$set_dir" -o "$APP/Contents/Resources/gtetrinet-ng.icns"
  icon_key="<key>CFBundleIconFile</key><string>gtetrinet-ng</string>"
fi

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>GTetrinet NG</string>
  <key>CFBundleDisplayName</key><string>GTetrinet NG</string>
  <key>CFBundleIdentifier</key><string>io.github.jarvis-jhk.gtetrinet-ng</string>
  <key>CFBundleExecutable</key><string>gtetrinet-ng</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.puzzle-games</string>
  <key>NSHighResolutionCapable</key><false/>
  $icon_key
</dict>
</plist>
PLIST
plutil -lint "$APP/Contents/Info.plist"

cp "$ROOT/CHANGELOG.md" "$ROOT/COPYING" "$ROOT/README.md" "$ROOT/THIRD_PARTY.md" "$APP/Contents/Resources/"
cp "$SDL/SDL2-license.txt" "$APP/Contents/Resources/"

# Apple Silicon refuses to run unsigned code, and install_name_tool broke the linker's signature.
codesign --force --sign - "$APP/Contents/Frameworks/$SDL_LIB"
codesign --force --sign - "$APP/Contents/MacOS/gtetrinet-ng-server"
codesign --force --sign - "$APP"
codesign --verify --deep --strict "$APP"

# Nothing may still point into the build machine (Homebrew, the SDL prefix, the build tree).
for bin in "$game" "$APP/Contents/MacOS/gtetrinet-ng-server" "$APP/Contents/Frameworks/$SDL_LIB"; do
  lipo "$bin" -verify_arch arm64 x86_64
  # Skip otool's per-slice "file (architecture x):" headers, keep the load commands.
  if otool -L "$bin" | awk '!/:$/ {print $1}' | grep -Ev '^(/usr/lib/|/System/Library/|@rpath/libSDL2)'; then
    echo "bundle-macos: $bin links outside the bundle and the system" >&2
    exit 1
  fi
done
