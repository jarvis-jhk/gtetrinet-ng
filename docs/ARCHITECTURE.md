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
the seat, and a new unclaimed controller can take it over with A. Physical/hotplug tests
remain a release follow-up: demonstration bots do not exercise controller dispatch.
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
