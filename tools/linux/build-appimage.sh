#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Wraps the finished portable tree (dist/gtetrinet-ng, as packed into the tar.gz) into a
# single-file AppImage. The static type2 runtime needs no libfuse2, which SteamOS lacks.
# Usage: build-appimage.sh <portable tree> <output.AppImage> <appimagetool> <runtime>
set -euo pipefail

TREE=$(cd "$1" && pwd)
mkdir -p "$(dirname "$2")"
OUT=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
TOOL=$(readlink -f "$3")
RUNTIME=$(readlink -f "$4")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
APPDIR="$WORK/GTetrinet_NG.AppDir"
mkdir -p "$APPDIR"
cp -a "$TREE" "$APPDIR/gtetrinet-ng"
# Installer and Steam helpers make no sense inside a read-only image; the launcher stays,
# because it reports crashes (it never updates outside ~/.local/share/gtetrinet-ng).
rm -f "$APPDIR/gtetrinet-ng/gtetrinet-ng.sh"

cat > "$APPDIR/AppRun" <<'RUN'
#!/bin/sh
G="$(dirname "$(readlink -f "$0")")/gtetrinet-ng"
if command -v python3 >/dev/null 2>&1; then
    exec python3 -I "$G/tools/linux/launcher.py" "$@"
fi
LD_LIBRARY_PATH="$G/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" exec "$G/bin/gtetrinet-ng" "$@"
RUN
chmod +x "$APPDIR/AppRun"

cp "$TREE/artwork/steam_icon.png" "$APPDIR/gtetrinet-ng.png"
cat > "$APPDIR/gtetrinet-ng.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=GTetrinet NG
Comment=TetriNET for up to eight players on one screen
Exec=gtetrinet-ng
Icon=gtetrinet-ng
Categories=Game;BlocksGame;
Terminal=false
DESKTOP

# appimagetool writes the .zsync into the working directory; keep it next to the AppImage.
cd "$(dirname "$OUT")"
# Update information lets AppImageUpdate and Gear Lever (Steam Deck) fetch new releases.
ARCH=x86_64 "$TOOL" --appimage-extract-and-run --no-appstream --runtime-file "$RUNTIME" \
    -u "gh-releases-zsync|jarvis-jhk|gtetrinet-ng|latest|gtetrinet-ng-x86_64.AppImage.zsync" \
    "$APPDIR" "$OUT"
