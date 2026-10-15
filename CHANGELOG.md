# Changelog

## 0.1.4 — 2026-10-15

Controller buttons can be rearranged: Options → Controller buttons lists the eight
in-game actions; pick one, press A and then the button it should use. A button that is
already taken swaps with the old one, Y restores the standard layout, and the choice is
saved for every controller. Movement, Start and the menus stay where they are, and the
keyboard keeps its classic keys. German translation included.

## 0.1.3 — 2026-10-13

German translation of the game: menus, hints, special names and effect labels, status
and connection errors, and the lobby/attack messages. Options (controller X / keyboard S
on the title screen) now holds Movement repeat and Language: Automatic, English or
Deutsch, saved like the repeat preset. The font gained Latin-1 letters (ä ö ü ß and
others); nicknames from classic clients that send Latin-1 now show instead of "?".
Typographic dashes and quotes in the changelog render, and its paragraphs reflow to the
screen width. The changelog text itself stays English. Small fixes: the title's empty
player cards no longer overflow, and the lobby's winner list no longer overlaps the
start prompt with eight players.

## 0.1.2 — 2026-10-12

Steam Deck feedback: clickable desktop installer with install/error dialogs, friendly
GTetrinet NG — TetriNET title and original matching Steam artwork. Existing Steam shortcut
IDs are preserved on rename; shortcut writes still require Steam to be fully closed.
Menus, nicknames and status text reduce font scale before abbreviation; help is shorter
and changelog headings wrap at their actual font size. Both rotation directions are now
explicit in the instructions and covered by live-session controller tests.

German translation, effect colors and a TetriNET 2 feature audit are on the roadmap.
Jan verified one-player play on a physical Deck; eight-controller hardware remains unverified.

## 0.1.1 — 2026-10-11

First modern preview of GTetrinet: SDL2 frontend, keyboard/gamepad input and up to eight
local fields; embedded offline server and standalone LAN server; TetriNET/TetriFast
protocol core, teams, specials and win lists. Linux/Steam Deck and Windows portable builds.
Linux installer checks for updates and shows this changelog after an upgrade.

Still experimental: external-server interoperability and real eight-controller Steam Deck
play need verification. Original GTK2 source and translations remain in legacy/.
