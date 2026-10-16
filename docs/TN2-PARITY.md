# TetriNET 2 parity

TetriNET 2 (St0rmCat, 2000/2001) is a separate game: its own client, its own protocol and
a central master server (tnet2.org). gtetrinet-ng is a TetriNET **1.13** client and
server. This page lists what TetriNET 2 added, what we have, and in which order the rest
is worth doing.

Sources: the Hard Drop wiki page "Tetrinet2" and the Wikipedia article "TetriNET"
(TetriNET2 section). Neither documents the protocol, and we have not found a
specification, so exact TN2 timings and shapes are not known; where we had to choose,
the choice is written down below.

## What "TetriNET 2 rules" means here

Options → **Rules for games here** → *TetriNET 2* switches games hosted on this machine
(and `gtetrinet-ng-server --tn2`) to:

- the hold piece (controller RT or right stick click, keyboard C or Shift; once per piece),
- the five TetriNET 2 specials next to the nine classic ones.

This is an **extension of our own server**, not TetriNET 2 compatibility. The server adds
`tn2 <hold> <frequencies>` after the eleven classic `newgame` fields, which classic
clients ignore; the new specials travel as ordinary `sb` messages with the letters
`i v m d f`, and a field holding one is sent whole (the classic diff format can only name
fifteen cell kinds). A client never accepts the new specials from a server that did not
announce the rules, and our online games against classic servers stay classic. A classic
client joining a TN2-rules game on our server does not know the new cells and specials
(how it shows them is untested), so play classic rules with classic clients.

## The list

Status: **done**, **partial**, **open**. Priority is for open items, highest first.

### Rules

| Feature | TN2 | Here | Status / priority |
|---|---|---|---|
| Field 12×22, one next piece, hard drop | yes | yes | done (classic) |
| Ghost piece ("block shadow") | yes | yes, every game | done |
| Hold piece | host option | TN2 rules | done |
| Specials collected per line cleared, used in inventory order | yes | yes | done (classic) |
| Entry delay (ARE) set by the host | yes | 1 s classic, 0 TetriFast | open, 2 |
| Specials on/off ("pure"), lines per special | host option | server rules exist, no UI | open, 1 |
| Piece frequencies set by the host | yes | server rules exist, no UI | open, 3 |
| Shuffle the inventory (costs one special) | yes | no | open, 4 |
| 180° rotation | no | no | not in TN2 |

### Specials

| Special | Letter | TN2 default | Here | Status |
|---|---|---|---|---|
| The nine classic ones | a c n r s b g q o | — | classic behaviour | done |
| Immunity | i | 15 s | 15 s; blocks every special from others, switches onto an immune player are refused on both ends; a green frame shows it to everyone | done |
| Clear Column | v | random column | one random column emptied | done |
| Mutate Pieces | m | next 3 pieces | the next three pieces are random connected shapes of five or six cells (TN2's own shapes are not documented) | done |
| Darkness | d | 10 s | 10 s; only the falling piece and two cells around it stay visible | done |
| Confusion | f | 10 s | 10 s; left, right and both rotations are reshuffled (never back to normal) | done |

The new specials are coloured by effect like the classic ones: Immunity helps, Mutate,
Darkness and Confusion hurt, Clear Column is mixed (fewer blocks, but a well through every
row). Timers show on the player's board with their seconds left.

### Modes, lobby and chat

| Feature | TN2 | Here | Status / priority |
|---|---|---|---|
| Teams, win list | yes | yes | done (classic) |
| Pure / 7-Tetris style modes | server-dependent | no | open, with "specials on/off" |
| Partyline chat with typing | yes | log only | open — roadmap M2 partyline item |
| Rooms on a central master server | yes | server browser for classic servers | partial; the TN2 master server is not a goal |

### Protocol and servers

| Feature | Status |
|---|---|
| Play on TetriNET 2 servers / with the TetriNET 2 client | **not supported**. No public protocol description; would need reverse engineering of the TN2 client. Not planned until a spec or a live server with users exists. |
| TN2 rules between gtetrinet-ng clients | done (the extension above) |
| Never advertise TN2 support from classic 1.13/TetriFast support | rule for README and release notes |

## Tests

`tests/test_core.c` (`test_tn2_*`): the `newgame` suffix round trip and its classic
prefix, whole-field updates with the new cells, hold (refused under classic rules, once
per piece, swap back, held mutants keep their shape), Mutate/Clear Column, all five new
specials appearing on fields, and over real TCP through our server: Immunity blocking an
add line and a switch, Darkness and Confusion timers and the confused controls, Mutate
and hold on a client, a classic client ignoring TN2 specials, and a four-bot TN2 game.
`tests/test_i18n.c` renders TN2 boards (one, two and eight players), the lobby and the
Options row in English and German at 1280×800 without cut text.
