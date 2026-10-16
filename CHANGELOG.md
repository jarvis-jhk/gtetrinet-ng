# Changelog

## 0.1.15 — 2026-10-17

The installer speaks German. On a German desktop, or once the game's language is set to
Deutsch, the Linux and Steam Deck installer asks, warns and confirms in German (with Ja/Nein
buttons), and its menu entries carry German names. The Windows Play.cmd now shows a short
message when the game ends with a fault or cannot start, in German or English.

## 0.1.14 — 2026-10-16

The original GTetrinet look. Options → Block style → Original GTetrinet draws the blocks,
the lettered specials and the shaded field backdrop from GTetrinet's own default theme
(the blocks.png that shipped with it, now built into the game), in every field and at
every size; small fields use the theme's small blocks. Specials keep a thin frame in their
effect color, so help and harm still read apart. The TetriNET 2 specials, which the old
theme never had, keep the modern look. Modern stays the default.

## 0.1.13 — 2026-10-16

Server limits for hosting on the internet. `gtetrinet-ng-server` (and a game hosted here)
now keeps serving its players when somebody hostile connects: one address can hold at most
8 of the 32 places for connections that have not logged in yet, a connection gets 32 query
commands, a player's messages are read at 100 per second (a flood waits in its own queue
and does not drown the chat or the game of the others) and a player sending more than
that, or not reading what is sent to them, is dropped. Levels from the network are kept
between 0 and 100. A new fuzz test throws half a million mangled messages at every parser
and attacks a running server with random bytes, floods, endless lines and connection spam
while two players keep playing.

## 0.1.12 — 2026-10-16

IPv6. You can type an IPv6 address in "Type an address..." (`[2001:db8::7]:31457`, or a
bare address for the usual port), and server names that only have an IPv6 address work
too. When a name has several addresses, the next one is tried if one does not answer
(IPv4 first, because classic servers check the login against their IPv4 address). A LAN
server (`gtetrinet-ng-server`, or a game hosted here) now takes IPv6 players as well as
IPv4 ones on the same port. Looking up a server name no longer freezes the screen: the
connecting screen says "Looking up ..." until the name server answers, and B cancels it.

## 0.1.11 — 2026-10-16

TetriNET 2 rules. Options → Rules for games here → TetriNET 2 gives games on this machine
the hold piece (right trigger or right stick click, keyboard C or Shift, once per piece) and
the five specials TetriNET 2 added: Immunity (I, 15 seconds in which nothing from others
reaches you), Clear Column (V), Mutate Pieces (M, the next three pieces come out big and
odd), Darkness (D, 10 seconds in which you only see around your piece) and Confusion (F, 10
seconds of reshuffled moves and rotations). The timers show next to your board, an immune
player gets a green frame, and computer players know what to do with the new specials.
The rules only apply between gtetrinet-ng players on a game hosted here (or on
`gtetrinet-ng-server --tn2`); online games on classic servers stay classic, and this is not
compatible with the original TetriNET 2. docs/TN2-PARITY.md lists what TetriNET 2 had and
what is still missing. German translation included. The right trigger no longer doubles
as RB; the left trigger still doubles as LB.

## 0.1.10 — 2026-10-16

An AppImage. Every release now also comes as one file, `gtetrinet-ng-x86_64.AppImage`:
download it, make it executable and open it — nothing to install, no libfuse2 needed, so
it also runs on the Steam Deck. AppImageUpdate and Gear Lever can update it from the
releases. The archive, the Deck installer and the Windows ZIP stay as they were.

## 0.1.9 — 2026-10-16

A server list. "Play online" now opens a list of the servers you played on and three
public TetriNET servers (tetrinet.de, tetrinet.fr, tetrinet.us). Each one is asked in the
background who is there: how many are playing, how many channels it has and how quickly
it answers, and for the chosen server its version and busiest channels. Servers that do
not tell (and those that cannot be reached) say so. Pick one with A or Enter; X (S) asks
again, Y (N) switches between classic and TetriFast, and "Type an address..." at the end
still lets you enter any server. A server goes into the list only once somebody got on
it. gtetrinet-ng's own server now answers these questions too, so other server lists can
show it, and `gtetrinet-ng-server --query HOST` asks a server from the command line.
German translation included.

## 0.1.8 — 2026-10-16

Playing on other TetriNET servers. Every build is now tested against two servers we did
not write, tetrinetx and tetrinet-server: logging in, chat, teams, specials, wins, full
servers and taken names all work there, and several players from one screen can join the
same online game. Neither server knows TetriFast, so choosing it there now says so (press
Y to switch to classic) instead of just "connection closed". If nobody could connect, every
player keeps their seat for the next try. Leaving a tetrinet-server game with several
players at once no longer crashes that server. The lobby no longer opens before a player
who had to take another name is in. German translation included.

## 0.1.7 — 2026-10-16

Computer opponents. In the lobby of a game on this machine, X (S on the keyboard) adds a
computer player to the next free seat and LB (Q) removes one again; RB (E) switches their
strength between Easy, Normal and Hard, and the choice is saved. Computer players move,
clear lines and use specials like everyone else, so one person can now play a full game
alone, and they stay seated from game to game until you leave. They only join games on
this machine, never an online server. German translation included.

## 0.1.6 — 2026-10-16

Sound and rumble. The game now plays short sound effects — a piece landing, one to four
cleared lines, using a special, being hit by one (harmful, helpful and mixed effects each
sound different), topping out and winning — and the controller of the player concerned
rumbles when they are hit, clear four lines, top out or win. Options has two new rows:
Sound effects (Off, Quiet, Normal, Loud) and Controller rumble (On, Off), both saved.
The sounds are generated by the game itself, so there are no audio files.
German translation included.

## 0.1.5 — 2026-10-16

Every player can choose their own name: after joining, press Y on the controller (N on
the keyboard) on the title screen and type it on the on-screen keyboard, or with a real
keyboard. Names are one word of up to 15 characters, are saved per seat P1–P8 and are
what the lobby, the boards and online servers show. Seats without a name of their own
keep following P1's name with a number. German translation included.

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
