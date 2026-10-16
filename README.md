# gtetrinet-ng

A modern continuation of [GTetrinet](https://gitlab.gnome.org/Archive/gtetrinet), the GPL
TetriNET client. Play falling-block battles locally, on a LAN, or connect to a TetriNET
server. The main goal is eight controllers sharing one Steam Deck screen.

**This is an early preview.** Eight client connections and scripted matches are tested,
and every build plays against two independent TetriNET servers (tetrinetx 1.13.16 and
tetrinet-server 0.11) in CI; physical eight-controller Deck play is still awaiting
verification. Classic servers/clients support six players, and those two servers do not
support TetriFast — use classic mode there.

![Eight-player scripted match at 1280×800](docs/eight-player-demo.png)

## Install

Download from [Releases](https://github.com/jarvis-jhk/gtetrinet-ng/releases).
Linux x86_64: extract the archive and run `./gtetrinet-ng.sh`. SDL2 is bundled; the host
supplies graphics/audio libraries (SteamOS, Mint or Ubuntu 22.04+). Python 3 and curl are
required by the installer/launcher. Windows x86_64: extract the ZIP, run `Play.cmd`;
`gtetrinet-ng.exe` also works directly, without launcher fault reporting.

AppImage (any Linux x86_64, no installation): download `gtetrinet-ng-x86_64.AppImage`, mark
it executable (`chmod +x`, or Properties → Executable) and open it. It needs no libfuse2.
It does not update itself; AppImageUpdate or Gear Lever (Steam Deck) can, using the embedded
update information. Add it to Steam with "Add a Non-Steam Game" if you want it in Game Mode.

Steam Deck: switch to Desktop Mode and download `Install-GTetrinet-NG.desktop` from the
release. Open it (confirm “Execute”/“Trust” if your desktop asks) and follow the install
dialogs. Fully exit Steam when prompted, so its shortcut can be safely added or renamed.
The friendly library title is **GTetrinet NG — TetriNET**, with original game artwork.
Alternatively run `bash install.sh`. It installs under `~/.local/share/gtetrinet-ng`, creates
a desktop shortcut and adds a Steam shortcut when Steam's userdata is found. Restart
Steam and return to Game Mode. Use the normal Gamepad template (Steam Input's keyboard
emulation consumes the Deck as a keyboard player). Extra controllers press A to join.
The Linux launcher checks for updates at startup; offline/failure keeps the current version.
Set `GTN_AUTO_UPDATE=0` to disable checks, `GTN_NO_REPORT=1` to disable anonymous reports.
Uninstall: remove that install directory and desktop/Steam shortcuts manually.

## Play

Press Enter (keyboard) or A (controller) to join. Select Local game; other devices can
join before the match starts. A/Start starts it. Each seat gets an independent nickname
and board. Controllers: d-pad/stick moves, Down soft-drops, Up hard-drops, A rotates clockwise / B counterclockwise,
X uses a special on the selected target, Y uses it on yourself, shoulders select targets,
Back discards a special, Start pauses, Back+Start ends the game. A disconnected seat can
be reclaimed by pressing A on an unclaimed controller. Eight fields are a dense layout.
Under TetriNET 2 rules the right trigger (or a right stick click) holds a piece.
These face, shoulder, Up, Back and right-trigger buttons can be rearranged under Options → Controller buttons.

TetriNET 2 rules: Options → Rules for games here → TetriNET 2 makes games hosted on this
machine use the hold piece and the five TetriNET 2 specials — Immunity (15 s, nothing from
others reaches you), Clear Column, Mutate Pieces (three odd five- or six-cell pieces),
Darkness (10 s, only the piece and its surroundings visible) and Confusion (10 s, moves and
rotations reshuffled). This is our own extension between gtetrinet-ng players: it is not
compatible with the TetriNET 2 client or its servers, and online games on classic servers
stay classic. `gtetrinet-ng-server --tn2` does the same for a LAN server. What TetriNET 2
had and what is still missing: [docs/TN2-PARITY.md](docs/TN2-PARITY.md).

Keyboard: arrows move/rotate, Space drops, X rotates clockwise / Z counterclockwise, S uses a special, Q/E or Tab
changes target, 1–8 uses it on a numbered player, D discards, C or Shift holds (TetriNET 2 rules), P pauses. `--nick Name`
sets your nickname.

Playing online: "Play online" lists the servers you played on (up to six, newest first)
and the public servers tetrinet.de, tetrinet.fr and tetrinet.us. Each is asked in the
background, with the TetriNET query protocol that tetrinetx and its descendants speak, how
many are playing and which channels it has; servers without that protocol show as
"online" with their response time. A/Enter connects, X/S asks again, Y/N switches
classic/TetriFast, B goes back. "Type an address..." opens an address entry with a
controller keyboard (`host`, `host:port`, or an IPv6 address as `[addr]:port`). Everybody who joined plays on that server,
each as their own player, in the server's first channel. Our own server answers the
same queries; `gtetrinet-ng-server --query HOST[:PORT]` shows what a server says.

Names: after joining, press Y on your controller (N on the keyboard) on the title screen
to type your name with the on-screen keyboard (Y switches capitals, X deletes, Start
saves, B cancels); a keyboard can type directly. A name is one word of up to 15
characters and is saved for that seat (P1–P8), so it comes back the next time someone
joins there. Seats without their own name use P1's name with a number.

Computer opponents: in the lobby of a game on this machine ("Play here"), X (keyboard S)
adds a computer player, LB (Q) removes the last one and RB (E/Tab) cycles their strength
— Easy (slow, misplaces a piece now and then), Normal or Hard — which is saved. They take
free seats up to eight, move and clear lines, use specials and keep their seats between
games until the session ends. They are never added to someone else's online server.

On the title screen, controller X or keyboard S opens Options, even before joining.
Up/Down picks a row; Left/Right or A changes it, Y restores its default and
B/Esc/Start returns. All choices are saved; a failed save shows a warning and keeps
the choice for the current run.

- **Movement repeat** cycles Normal (170 ms delay, 50 ms interval), Relaxed
  (250/100 ms) and Fast (120/30 ms) for held Left/Right on every local
  keyboard/controller. Missing or invalid preferences keep Normal. Soft drop and
  rotation keep their own behavior.
- **Language / Sprache**: Automatic (the system's preferred language), English or
  Deutsch. The German UI covers menus, hints, special names and effect labels,
  status/error messages and the game's own lobby/attack messages. Changelog entries,
  chat, server messages, team names and operating-system error texts stay as written.
  `--lang de|en|auto` overrides it for one run without saving.
- **Controller buttons** opens a list of the eight in-game actions (rotate clockwise and
  counterclockwise, hard drop, special on target / on yourself, previous / next target,
  discard). Pick one, press A, then press the controller button it should use; the action
  that had that button takes the old one, so nothing is ever unbound. Y restores the
  standard layout. Up, A, B, X, Y, LB, RB and Back can be assigned (the left stick's up
  and the triggers follow Up and LB/RB). Left/Right/Down, Start, Back+Start and all menus
  stay fixed. The layout is shared by every controller and is used only in a game; the
  keyboard keeps its classic keys.
- **Sound effects**: Off, Quiet, Normal or Loud. The game plays short synthesized sounds
  (no audio files) when a piece lands, for one to four cleared lines, when a special is
  used, when one hits (harmful, helpful and mixed effects sound different), on topping
  out and on winning. The screen is shared, so the same sound plays once even when
  several players trigger it at the same moment. Changing the level plays a sample.
- **Controller rumble**: On or Off. The controller of the player concerned rumbles when
  a harmful special hits them (a light tap for a helpful one), when they clear four lines,
  top out or win — on a shared screen it is how you know it was you. The keyboard has none.

Special tiles keep their classic letters and show their effect on the **recipient**:
green `+HELP` (C clear line, N nuke field, G gravity), red `-HURT` (A add line),
amber `?MIX` (R random clear, S switch fields, B clear specials, Q quake, O bomb).
The next queued special has a symbol and category word below the inventory, so color
is not the only cue. Very small field/queue tiles cannot fit a letter; the next-special
label still identifies it. Under TARGET, `+You`/`-You` or `+Them`/`-Them` describes
the selected recipient; `?MIX` means inspect their field, and `?Both` means a swap
changes both players' fields (`?Self` when aimed at yourself).

Helping yourself or a teammate usually helps your side; helping an opponent helps
them. Adding a line hurts the chosen recipient, even yourself. Random clears can
open holes; clearing specials prevents collecting them but can also remove bombs;
quakes shift rows, and bombs scatter nearby cells. Swaps trade the sender's and
target's fields with the classic six-row safety rule. These categories are guidance,
not a guarantee of a win or loss. X/S sends to the selected target (automatically
selecting a live player if necessary); Y always uses the next special on yourself.
Aim with the shoulders/Q/E before sending. Colors never change targeting or the
classic wire protocol.

LAN server: `bin/gtetrinet-ng-server --port 31457 --slots 6` (Windows: server EXE).
Our local game hosts on 31457, or a free port if busy; both listen on IPv6 and IPv4. Firewall/router configuration is
your responsibility. Only use eight slots with clients that support our extension.
No public-server hardening guarantee is made for this preview.

## Build

CMake 3.16+, a C11 compiler and SDL2 2.0.14+ development headers. On Debian/Ubuntu install
`cmake build-essential libsdl2-dev`; on Windows use MSYS2 UCRT64 + GCC/CMake/SDL2.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/gtetrinet-ng --window
```

For server/core-only builds use `-DGTN_BUILD_GAME=OFF`. Demo/smoke capture:
`./build/gtetrinet-ng --window --demo 8 --frames 600 --screenshot demo.bmp`.
These are real TCP bot matches, not hardware-input tests.

## Maintenance, privacy and licence

JARVIS maintains this fork; roadmap goals are in [ROADMAP.md](ROADMAP.md), current
validation/limitations in [NOTES.md](NOTES.md). Report problems via GitHub issues.
Launcher failures send only an anonymous version/platform/exit-code report through
ntfy.sh into a GitHub issue. No logs or user/device details are uploaded; topic messages
and issues are public. Release binaries also post a one-line anonymous fault; source builds post nothing unless
configured with `-DGTN_FAULT_REPORTS=ON`. `GTN_NO_REPORT=1` disables all reporting.

GPL-2.0-or-later; see COPYING. Full upstream history, GTK2 source and translations are
preserved in legacy/. Original GTetrinet authors retain their attribution. The active
frontend uses the public-domain font8x8 glyphs (third_party/font8x8_basic.h).
