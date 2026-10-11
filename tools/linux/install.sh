#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
GTN_TAG="@TAG@"
dialog() {
    local kind=$1 text=$2
    if command -v zenity >/dev/null; then zenity --"$kind" --no-wrap --text="$text"
    elif command -v kdialog >/dev/null; then
        case "$kind" in question) kdialog --yesno "$text";; error) kdialog --error "$text";; *) kdialog --msgbox "$text";; esac
    elif [ "$kind" = question ]; then
        local answer
        read -rp "$text [y/N] " answer && [[ "$answer" == [yY]* ]]
    else printf '%s\n' "$text"; fi
}
fault() {
    # Only a fixed stage name leaves the device, never paths or diagnostic output.
    if [ "${GTN_NO_REPORT:-0}" != 1 ]; then curl -fsS --max-time 8 -H 'Title: gtetrinet-ng installer fault' \
        --data-binary "gtetrinet-ng $GTN_TAG Linux: installer $1" \
        https://ntfy.sh/gtetrinet-ng-faults-5e8b41e03cf04b57 >/dev/null 2>&1 &
    fi
    dialog error "Installation failed ($1). Please try again." || true
    exit 1
}
for tool in python3 curl; do command -v "$tool" >/dev/null || { dialog error "Install $tool first"; exit 1; }; done
dialog question 'Install GTetrinet NG — TetriNET on this screen? This installs the game, updates, menu entry and Steam artwork.' || exit 0
GTN_TMP=$(mktemp -d)
trap 'rm -rf "$GTN_TMP"' EXIT
# Bootstrap from the same release as this installer, not an unreviewed moving branch.
curl -fsSL --max-time 60 "https://github.com/jarvis-jhk/gtetrinet-ng/releases/download/$GTN_TAG/launcher.py" -o "$GTN_TMP/launcher.py" || fault download
python3 "$GTN_TMP/launcher.py" --install || fault bootstrap
GTN_ROOT="$HOME/.local/share/gtetrinet-ng/current"
mkdir -p "$HOME/.local/share/applications"
cat > "$HOME/.local/share/applications/gtetrinet-ng.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=GTetrinet NG — TetriNET
Comment=Local and online falling-block battles for up to eight players
Exec="$GTN_ROOT/gtetrinet-ng.sh"
Icon=$GTN_ROOT/artwork/steam_icon.png
Terminal=false
Categories=Game;
DESKTOP
if pgrep -x steam >/dev/null; then
    dialog question 'The game is installed. Fully exit Steam now, then click Yes to add or update its shortcut. No skips adding it.' || {
        dialog info 'Installed in your applications menu. Run this installer again with Steam closed to add the Steam shortcut.'
        exit 0
    }
fi
if python3 "$GTN_ROOT/tools/linux/add-to-steam.py" 'GTetrinet NG — TetriNET' "$GTN_ROOT/gtetrinet-ng.sh" "$GTN_ROOT" ''; then
    dialog info 'Installed with Steam artwork. Start Steam again and return to Game Mode. Use the Gamepad template; extra controllers press A to join.'
else
    dialog error 'The game is installed in your applications menu. Steam shortcut could not be added: fully exit Steam and run this installer again, or add the game manually.'
fi
# END OF GTETRINET INSTALLER
