# Roadmap

Jan edits goals; JARVIS owns delivery. Checkboxes mean implemented and locally verified,
not certified hardware compatibility. Release gates and remaining checks are in NOTES.md.

## M0 — first modern, installable build (0.1.1 preview)

- [x] Preserve original GTK2/GNOME2/ESD source under legacy/ and full history.
- [x] C11 portable core, SDL2 rendering/gamepads, Linux and Windows builds.
- [x] Embedded offline server, standalone LAN server, keyboard play.
- [x] Game rules, specials, teams, win list, protocol and real TCP integration tests.
- [x] Linux/Windows CI, published portable assets and verified download.
- [x] Steam Deck installer, Steam shortcut, update checks and in-game changelog.
- [x] Anonymous fault relay via ntfy, GitHub issues and private queue secret.

## M1 — eight controllers on the Deck

- [x] Eight local client slots and responsive board layout; hotplug/takeover input.
- [x] SDL virtual-device tests (simulated event tests already cover routing): independent ownership, join/leave,
  held-input disconnect, reconnection, device replacement and repeated slot reuse.
- [ ] Physical Steam Deck + eight-controller play session; suspend/resume, Game Mode,
  dock/undock and 1280×800 legibility. No hardware certification until this passes.
- [ ] Controller remapping, repeat settings, nickname editing and accessible typography.
  - [x] Saved Normal/Relaxed/Fast horizontal-repeat presets, controller/keyboard options
    entry and real TCP/input timing tests.
  - [x] Shared in-game controller button layout (eight actions over Up/A/B/X/Y/LB/RB/Back,
    swap-on-conflict, saved, Options → Controller buttons) with real TCP play tests;
    per-player layouts, keyboard remapping and typography remain open.
  - [x] Nickname editing per seat (title screen Y/N, on-screen keyboard or typing, one word
    up to 15 characters, saved per seat, used in the lobby and online) with real TCP tests.
- [x] Sound, haptics and offline bot opponents selectable in the lobby.
  - [x] Synthesized sound effects (lock, 1–4 line clears, special used, harmful/helpful/
    mixed hits, top-out, win) and per-player controller rumble, with Sound effects and
    Controller rumble rows in Options; real TCP game tests. Physical rumble feel unverified.
  - [x] Offline bot opponents selectable in the lobby: X adds, LB removes, RB cycles
    Easy/Normal/Hard (saved); local games only; real TCP game test. Play feel unverified.

## M2 — online interoperability

- [x] Independent server fixture (tests/interop.sh, CI job `interop`): our client and the
  game itself against unmodified tetrinetx 1.13.16 and tetrinet-server 0.11 — login, chat,
  teams, fields, specials, wins, taken nicknames, full six-slot servers, four local players
  joining one online game, TetriFast refusal explained to the player.
  - [ ] Legacy GTetrinet/TetriNET 1.13 client on our server, moderator hand-over when the
    moderator leaves, and a public TetriFast-capable server once one is found.
- [x] Server browser: recent and public servers asked in the background (TetriNET query
  protocol: players, channels, version, response time), pick to connect, typed address
  still possible; our server answers the queries too (`--query` on the command line).
  Real TCP test (test_browser) and the interop fixture query all three servers.
- [ ] Partyline/chat UI, channel choice, profiles and reconnect policy.
- [ ] Warpzone (github.com/jarvis-jhk/warpzone): a hosted local game as a Warpzone room
  (tier 2), guests over the Warpzone peer channel so nobody forwards ports, the declared
  ranked FFA queue (up to 6), and the built-in updater silent in `.warpzone-managed`
  installs. Descriptor games/gtetrinet-ng.toml in that repo; no free-text player content.
- [x] Malformed-message fuzzing and limits for public-facing hosting (v0.1.13): per-address
  waiting cap, query cap, message rate limit, flood/slow-reader drop, tests/test_fuzz.c.
- [x] IPv6 and asynchronous DNS; avoid blocking the render loop on lookup (v0.1.12).

## M3 — distribution polish

- [ ] AppImage, Windows installer/update parity, signed releases where practical.
  - [x] AppImage: `gtetrinet-ng-x86_64.AppImage` in every release (static runtime, zsync update info).
- [ ] Optional crash-report consent UI and local diagnostic export (redacted).
- [x] German translation of the active game: menus, hints, specials, errors and changelog UI;
  UTF-8/Latin-1 font coverage (including ä/ö/ü/ß), saved language choice, and a rendered
  1280×800 layout test that fails on any cut text. Changelog entries stay English.
  Legacy GTK translations are reference material, not a translated modern frontend.
  - [ ] German Linux installer and Windows launcher dialogs.
  - [ ] Physical Deck legibility of the German UI (with the M1 hardware session).
- [x] Color-code specials by effect: beneficial, harmful and situational, with readable
  labels/symbols as well as colors; document how the chosen target changes the effect.
- [x] Audit missing TetriNET 2 features and prioritize a documented parity list (rules,
  specials, modes, lobby/chat and protocol). Distinguish gameplay additions from TN2 server
  compatibility; do not advertise TN2 support from classic 1.13/TetriFast support.
  docs/TN2-PARITY.md; TetriNET 2 rules for games hosted here (hold piece, Immunity, Clear
  Column, Mutate, Darkness, Confusion) as an extension between our clients.
  - [ ] Host settings from the parity list: specials on/off ("pure"), entry delay, piece
    frequencies, inventory shuffle.
  - [ ] TetriNET 2 server/client compatibility only if a protocol description turns up.
- [x] Two-way rotation exists: controller A clockwise / B counterclockwise, keyboard
  X clockwise / Z counterclockwise. Keep explicit help and verify both on physical Deck
  controls; configurable bindings and rotation behavior remain M1 work.
- [ ] Original upstream theme migration.

## Deck feedback — 2026-10-11

Jan reports that v0.1.1 works on a physical Steam Deck with one player. This is a
single-player hardware observation; eight-controller and suspend/dock checks remain open.
- [x] Prepare release packaging for a clickable `.desktop` installer with dialogs, friendly game title and matching
  original falling-block Steam capsule/hero/logo/icon, preserving existing shortcut appids.
- [x] Fix clipped menus, help, nicknames and news headings at Deck resolutions; verify
  title, online entry, lobby, news, pause and one/eight-player views separately (local rendering; physical recheck remains M1).
