#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
GTN_TAG="@TAG@"
# The game's own language choice wins (Options -> Language, saved by SDL's pref path),
# then the desktop locale in the order SDL reads it, so the installer and game agree.
gtn_german() {
    local saved="${XDG_DATA_HOME:-$HOME/.local/share}/gtetrinet-ng/gtetrinet-ng/settings.ini" choice=""
    [ -r "$saved" ] && choice=$(sed -n 's/^lang=//p' "$saved" | tail -n 1)
    case "$choice" in de) return 0;; en) return 1;; esac
    local v
    for v in "$(printf %s "${LANGUAGE:-}" | cut -d: -f1)" "${LC_ALL:-}" "${LC_MESSAGES:-}" "${LANG:-}"; do
        case "$v" in de|de_*|de.*|de@*) return 0;; ""|C|C.*|POSIX) ;; *) return 1;; esac
    done
    return 1
}
if gtn_german; then GTN_DE=1; else GTN_DE=0; fi
t() { if [ "$GTN_DE" = 1 ]; then printf '%s' "$2"; else printf '%s' "$1"; fi; }
dialog() {
    local kind=$1 text=$2
    if command -v zenity >/dev/null; then
        if [ "$kind" = question ]; then
            zenity --question --no-wrap --text="$text" --ok-label="$(t Yes Ja)" --cancel-label="$(t No Nein)"
        else zenity --"$kind" --no-wrap --text="$text"; fi
    elif command -v kdialog >/dev/null; then
        case "$kind" in question) kdialog --yesno "$text";; error) kdialog --error "$text";; *) kdialog --msgbox "$text";; esac
    elif [ "$kind" = question ]; then
        local answer
        read -rp "$text $(t '[y/N]' '[j/N]') " answer && [[ "$answer" == [yYjJ]* ]]
    else printf '%s\n' "$text"; fi
}
fault() {
    # Only a fixed stage name leaves the device, never paths or diagnostic output.
    if [ "${GTN_NO_REPORT:-0}" != 1 ]; then curl -fsS --max-time 8 -H 'Title: gtetrinet-ng installer fault' \
        --data-binary "gtetrinet-ng $GTN_TAG Linux: installer $1" \
        https://ntfy.sh/gtetrinet-ng-faults-5e8b41e03cf04b57 >/dev/null 2>&1 &
    fi
    dialog error "$(t "Installation failed ($1). Please try again." "Die Installation ist fehlgeschlagen ($1). Bitte versuche es noch einmal.")" || true
    exit 1
}
for tool in python3 curl; do command -v "$tool" >/dev/null || { dialog error "$(t "Install $tool first." "Bitte installiere zuerst $tool.")"; exit 1; }; done
dialog question "$(t 'Install GTetrinet NG — TetriNET on this screen? This installs the game, updates, menu entry and Steam artwork.' \
    'GTetrinet NG — TetriNET auf diesem Gerät installieren? Installiert werden das Spiel, Updates, ein Menüeintrag und Steam-Grafiken.')" || exit 0
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
Comment[de]=Fallende Blöcke gegeneinander, lokal und online, für bis zu acht Spieler
Exec="$GTN_ROOT/gtetrinet-ng.sh"
Icon=$GTN_ROOT/artwork/steam_icon.png
Terminal=false
Categories=Game;
DESKTOP
if pgrep -x steam >/dev/null; then
    dialog question "$(t 'The game is installed. Fully exit Steam now, then click Yes to add or update its shortcut. No skips adding it.' \
        'Das Spiel ist installiert. Beende Steam jetzt vollständig und klicke dann auf Ja, um die Verknüpfung hinzuzufügen oder zu aktualisieren. Nein überspringt das.')" || {
        dialog info "$(t 'Installed in your applications menu. Run this installer again with Steam closed to add the Steam shortcut.' \
            'Im Anwendungsmenü installiert. Starte diesen Installer bei geschlossenem Steam noch einmal, um die Steam-Verknüpfung hinzuzufügen.')"
        exit 0
    }
fi
if python3 "$GTN_ROOT/tools/linux/add-to-steam.py" 'GTetrinet NG — TetriNET' "$GTN_ROOT/gtetrinet-ng.sh" "$GTN_ROOT" ''; then
    dialog info "$(t 'Installed with Steam artwork. Start Steam again and return to Game Mode. Use the Gamepad template; extra controllers press A to join.' \
        'Mit Steam-Grafiken installiert. Starte Steam wieder und wechsle in den Spielmodus. Nimm die Gamepad-Vorlage; weitere Controller drücken A zum Mitspielen.')"
else
    dialog error "$(t 'The game is installed in your applications menu. Steam shortcut could not be added: fully exit Steam and run this installer again, or add the game manually.' \
        'Das Spiel ist im Anwendungsmenü installiert. Die Steam-Verknüpfung konnte nicht angelegt werden: Beende Steam vollständig und starte diesen Installer noch einmal, oder füge das Spiel von Hand hinzu.')"
fi
# END OF GTETRINET INSTALLER
