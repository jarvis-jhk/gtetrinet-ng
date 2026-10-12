# Architecture

The archived GTK2 client is retained unchanged under legacy/. Its GNOME2, GConf and
ESD dependencies are unsuitable for a Windows/Deck distribution, so the active frontend
is C11 + SDL2 rather than another GTK port. This avoids two rendering/input stacks.

src/core owns rules, deterministic RNG, the TetriNET 1.13 login and framed TCP messages,
client sessions and the server. It does not depend on SDL. src/app owns SDL rendering,
controller identity, per-player input repeat, preferences and screens. The font is the
public-domain font8x8 bitmap; no proprietary artwork is bundled.

Each local player has a separate client, socket, nickname, field and special inventory.
The embedded server uses exactly those TCP connections, so offline and LAN matches share
the same rules/message path. The standalone server defaults to six slots; the local game
uses eight. Standard clients normally display only six: use six slots when mixing clients.
The extension does not make an eight-player match interoperable with classic clients.

Controllers are identified by SDL instance id, not discovery index. A disconnect retains
the seat, and a new unclaimed controller can take it over with A. The `input_virtual`
CTest attaches SDL virtual gamecontrollers and drives their buttons/axes through SDL's
event queue, including actual attach/detach events. Eight independent TCP players,
concurrent held inputs, middle-seat leave/rejoin, capacity rejection and replacement
with preserved session identity are covered. This requires SDL 2.0.14 or newer.
Physical Deck/Steam Input, suspend/resume and dock/undock checks remain open;
virtual devices and demonstration bots cannot certify them.
SDL2 gamepad API reference: https://wiki.libsdl.org/SDL2/CategoryGameController

Packaging keeps a native binary and optional bundled SDL2 beside a launcher. Linux
installation uses versioned folders and atomic current-symlink replacement, validating
SHA256SUMS before extracting. Failed/offline updates keep the last installed game.
The launcher reports only a version, platform and failure/exit code to a public ntfy topic;
never raw application logs, nicknames, server addresses or filesystem paths. A scheduled
GitHub workflow deduplicates reports into issues; its private queue URL is a repo secret.
ntfy publishing reference: https://docs.ntfy.sh/publish/

This is a preview. External-server compatibility, public-host security hardening and
actual Deck/eight-device verification are open acceptance tasks, not proven by self-tests.
