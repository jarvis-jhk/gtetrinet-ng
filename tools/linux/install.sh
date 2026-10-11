#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
for tool in python3 curl; do command -v "$tool" >/dev/null || { echo "Install $tool first"; exit 1; }; done
GTN_TMP=$(mktemp -d)
trap 'rm -rf "$GTN_TMP"' EXIT
# Bootstrap from the same release as this installer, not an unreviewed moving branch.
GTN_TAG="@TAG@"
curl -fsSL --max-time 60 "https://github.com/jarvis-jhk/gtetrinet-ng/releases/download/$GTN_TAG/launcher.py" -o "$GTN_TMP/launcher.py"
python3 "$GTN_TMP/launcher.py" --install
GTN_ROOT="$HOME/.local/share/gtetrinet-ng/current"
mkdir -p "$HOME/.local/share/applications"
cat > "$HOME/.local/share/applications/gtetrinet-ng.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=gtetrinet-ng
Exec="$GTN_ROOT/gtetrinet-ng.sh"
Terminal=false
Categories=Game;
DESKTOP
if pgrep -x steam >/dev/null; then
    echo 'Game installed. Exit Steam, then run the installer again to add its shortcut.'
else
    python3 "$GTN_ROOT/tools/linux/add-to-steam.py" gtetrinet-ng "$GTN_ROOT/gtetrinet-ng.sh" "$GTN_ROOT" '' ||
        echo 'Steam shortcut was not added; add the game manually from the Games menu.'
fi
echo "Installed. Launch $GTN_ROOT/gtetrinet-ng.sh"
