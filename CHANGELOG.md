# Changelog

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
