# Running state — 2026-10-17

## Current: host settings released as v0.1.16 (work session t_mvbr1hfo39m4rn, item tetrinet-host-settings, done)

Goal: games hosted here can be set up as pure (no specials), with lines per special,
starting garbage and a piece mix; shown in the lobby; shipped in a release.
core: tn_rules_pieces/tn_rules_piece_mix + enum tn_piece_mix (classic tetrinetx 15/14..,
nosz I O J L T 20 each, bars I 28 rest 12), codes classic|nosz|bars. All via the classic
newgame fields (stack, lines per special, specials added, blockfreq), so classic clients get
them as well; no protocol change. settings.ini specials=on|off, special_lines=1..4,
garbage=0..10 even, pieces=<code>; gtn_host_rules() builds the hosted rules (replaces the
bare tn_rules_tn2 call in host_local). UI: Options row 5 Left/Right still switches the rule
set, Y resets rule set AND host settings, A opens SCR_RULES (rows rule set, specials, lines
per special, garbage, pieces; Y per row; B back to Options row 5). Options row value says
"<set>, custom" when any host setting differs. Lobby: the rules summary (rule set line, then
specials and garbage/pieces lines) is drawn at the top of the chat panel, which loses those
lines; the old right-header spot collided with the German port line. Server: --pure,
--special-lines N, --garbage N, --pieces MIX. tests/test_rules.c (ctest "rules"): mixes,
newgame round trip, 4000-draw piece counts (no S/Z; I ~600 classic vs ~1140 bars), pure and
3-lines-per-special clears, settings load/save/malformed, controller navigation, render at
800x600/1280x800 en/de with no cut text, and a real TCP game (two pads + a bot) whose clients
all get the rules, start with the garbage, and whose bot clears lines with no special and no
S/Z ever. Negative control (specials_off ignored in gtn_host_rules) fails 5 checks.
test_sound used A on row 5 for TN2; now Right. Local Debug+ASan/UBSan: all 14 ctests pass.
2830253 passed linux/windows/interop CI 39001804286 (rules ctest ran on both); tag v0.1.16
release 39002111767 passed. All assets downloaded, SHA256SUMS OK; packaged server 0.1.16
accepted --pure --special-lines 3 --garbage 6 --pieces nosz and answered a query; packaged
game ran headless with those settings.ini keys. Open (parity list): entry delay, free piece
percentages, inventory shuffle. Next release > v0.1.16.
https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.16

## Current: German installer dialogs released as v0.1.15 (item tetrinet-german-installer, done)

Opus filler t_mvbl5dbw1hmjg1. Goal: on a German system (or with Sprache Deutsch saved) the
released Linux installer, its launcher errors and the Windows launcher's fault dialog speak
German, English otherwise. Language rule everywhere: settings.ini lang=de|en in SDL's pref
dir (Linux ${XDG_DATA_HOME:-~/.local/share}/gtetrinet-ng/gtetrinet-ng/, Windows
%APPDATA%\gtetrinet-ng\gtetrinet-ng\), else LANGUAGE/LC_ALL/LC_MESSAGES/LANG (Linux) or
Get-UICulture (Windows). install.sh t() helper, zenity --ok-label/--cancel-label Ja/Nein
(kdialog keeps its own labels); launcher.py german()/t(); .desktop Name[de]/Comment[de] and a
bilingual download-failure line. play.ps1 is ASCII-only (PS 5.1 reads BOM-less as ANSI;
umlauts via [char]) and shows a MessageBox on exit codes other than 0/2; GTN_NO_DIALOG=1
prints instead. Tests: tests/test_deck_installer.py (en/de/saved/failure),
tests/test_launcher.py LanguageTests, CI windows step tests/test_play_launcher.ps1 with a
stub exe exiting 3. Known: a failing install.sh shows its own error and then the .desktop
wrapper's bilingual one (pre-existing double dialog).
2a4ca76: windows job failed only because the Actions PowerShell wrapper re-raised the stub's
exit code after all checks passed; 419552e ends the test with exit 0. 419552e passed
linux/windows/interop CI 38982226824 (the Windows step printed correct German incl. umlauts);
tag v0.1.15 release 38982708681 passed. All assets downloaded, SHA256SUMS OK; the published
install.sh run with LANG=de_DE in an empty home installed v0.1.15 ("Installiert: v0.1.15")
with German Ja/Nein question and German Steam-shortcut error (no Steam there).
Open: kdialog-only desktops keep kdialog's own button labels. Next release > v0.1.15.
https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.15

## Current: original GTetrinet theme released as v0.1.14 (item tetrinet-original-theme, done)

Opus filler t_mvbayspk9sttr. Goal: Options → Block style 'Original' draws every field, piece
and special with GTetrinet's own blocks.png, saved, shipped in a release.
render.c decodes legacy/themes/default/blocks.png (CMake writes generated/blocks_png.h from
it at configure time) with third_party/stb_image.h (PNG only) into one texture. Sheet: row 0
cells 1..14 at 20 px, row y=20 the same at 10 px, y=30 own-field backdrop 240x440, right of
it the small-field backdrop 120x220. Cells >= 14 px use the 20 px row, smaller the 10 px row;
fields >= 14 px the big backdrop. Grid lines off in original style. Specials 6..14 get a
1-2 px effect-color frame; TN2 specials 15..19 have no art and stay modern. Ghost piece uses
texture alpha. settings.ini blocks=original|modern (default modern), Options row 6;
Options at Deck height hides the repeat timing lines unless row 0 is chosen (seven rows).
tests/test_blocks.c: rendered pixels vs the decoded sheet (big + small blocks, backdrop,
frame color, TN2 fallback, modern unchanged) and the Options row/save; a wrong source
offset fails 744/1200 pixels. test_i18n got options-blocks, game1-orig and game8-orig views
(classic stack fixture, ingame=3). Only test_blocks and test_i18n were run here.
dc371ae passed linux/windows/interop CI 38948594108 (blocks ctest ran on both); tag v0.1.14
release 38949680684 passed. All assets downloaded, SHA256SUMS OK; the packaged game with
blocks=original drew the original blocks and backdrop. Item done. Next release > v0.1.14.
https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.14

## Current: fuzzing and server limits released as v0.1.13 (item tetrinet-fuzzing-limits, done)

Opus filler t_mvb4071afcmch. Goal: a released server keeps serving honest players while a
hostile client floods it with random bytes, oversized lines, unread output and connection
spam; a sanitizer fuzz test over every parser and the server runs in CI.
tn_server: pending 16 -> 32, max 8 per tn_tcp_peer_key (IPv4, or IPv6 /64), 32 queries per
pending connection, per-player token bucket (burst 200, 100/s, in 1/1000 units, dt clamped
10 s) -- when empty the server stops reading (TCP backpressure); a player whose 16 KiB input
buffer is full behind the limit is dropped as "flooding". tn_conn.outmax (server: 1 MiB,
default still 4 MiB). Network levels clamped 0..100 in server and client (average_levels
summed raw ints before). Standalone server now passes real elapsed time to update (was a
fixed 10 ms per loop, which made timeouts/rates run slow).
tests/test_fuzz.c (ctest "fuzz", UBSAN_OPTIONS=halt_on_error=1): 120k mangled/random
messages through login decode, field apply, rules parse (+ games played with the result),
tn_word, query parsers/split and tn_client_handle; then a live dual-stack server with two
honest clients vs a raw hostile player (mangled + random bytes), a 600-line chat flood
(299 relayed in 1 s of server time, honest chat still through), an endless line (dropped),
40 connections from 127.0.0.1 (<= 8 pending, ::1 player still joins), 200 queries (<= 32
answered, closed), then the honest pair plays. Found no memory error in the parsers under
ASan/UBSan. Negative control (no rate limit, no per-address cap) fails 2 checks.
Local Debug+ASan/UBSan: test_fuzz and test_core pass (only those were run here).
b39da46 passed linux/windows/interop CI 38923311822 (fuzz ctest ran on both: 4 s / 29 s);
tag v0.1.13 release 38923916689 passed. All eight assets downloaded, SHA256SUMS OK; the
packaged server (VERSION v0.1.13) refused 12 of 20 connections from one address and
answered 32 of 50 queries. https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.13
Open: a real internet-facing host under real abuse; not a security audit. Next release > v0.1.13.

## Current: IPv6 and asynchronous DNS released as v0.1.12 (item tetrinet-ipv6-async-dns, done)

Opus filler t_mvb2e9jz4tzqwa. Goal: a released build connects by IPv6 address/hostname
([v6]:port), the LAN server accepts IPv6 clients, a slow lookup no longer freezes the screen.
tn_net: tn_tcp_connect (blocking getaddrinfo, first address only) replaced by tn_connector:
getaddrinfo(AF_UNSPEC) on a detached pthread/CreateThread job with an atomic refcount (the
client can give up mid-lookup, the job frees itself), then every address in turn, IPv4 first
(classic login obfuscation keys on the server's IPv4; IPv6 peers get zeros, which our server
decodes anyway - it brute-forces the key), 3 s per address while more remain. No thread ->
synchronous fallback. tn_query uses it too. tn_tcp_listen: non-loopback binds a dual-stack
:: socket (IPV6_V6ONLY=0, Windows defaults to 1), IPv4 fallback; embedded/--local stay on
127.0.0.1. Connecting screen says "Looking up %s ..." (de: "Suche %s ...") while resolving.
Test hooks tn_net_test_resolve_delay_ms / tn_net_test_prefer_ipv6; test_core
test_ipv6_and_lookup (split forms, ::1 + 127.0.0.1 in one lobby, ::1-refused -> v4 fallback,
1.2 s delayed lookup with no frame >200 ms, cancel mid-lookup, .invalid fails). Negative
controls: synchronous lookup fails 3 checks, no fallback fails 1. Debug+ASan/UBSan ctest
11/11, python tests OK, MinGW test_core.exe builds. tetrinet.fr has AAAA 2001:41d0:8:6717::3 (de/us IPv4 only); the container has no IPv6 egress,
so a real internet IPv6 game is untested here - open for Jan on an IPv6 network.
871fe3f passed linux/windows/interop CI 38917487601; tag v0.1.12 release 38920030973 passed.
All assets downloaded, SHA256SUMS OK (AppImage too); packaged server answers --query on
[::1], 127.0.0.1 and localhost. https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.12
Announcement: jarvis_reply was tried 3x and every call was interrupted without a result; whether Jan got it is unknown. Do not resend blindly.
Next release > v0.1.12.

## Earlier: TetriNET 2 rules released as v0.1.11 (item tetrinet-tetrinet-2-parity, done)

Opus filler t_mvb097po3u2yg7. Goal: docs/TN2-PARITY.md lists TN2 features with status and
priority, and a released local game can switch on TN2 rules (hold + Immunity, Clear Column,
Mutate, Darkness, Confusion). Sources for TN2: Hard Drop wiki "Tetrinet2", Wikipedia
"TetriNET"; no protocol spec exists publicly, so TN2 wire compatibility is out of scope.
Extension: newgame gets " tn2 <hold> <sf>" after the 11 classic fields (sf chars 1-9 classic,
A-E new; the classic sf field carries classic specials only). Cells 15..19 = i v m d f; the
diff format can only name cells 0..14 ('!'+cell collides with full-field chars from '0'), so
a field with a TN2 cell change is sent whole. Clients drop TN2 sb ids unless rules.tn2.
Immunity is tracked per slot by every client (all sb messages are broadcast), so a switch
onto an immune player is refused on both ends. Mutants are block ids 7/8/9 (cur/next/hold)
with shapes stored in tn_game; tn_game_shape() replaces tn_block_shape() for game pieces.
Hold = BTN_RT (new; right trigger + right stick click, keyboard C/Shift), PAD_HOLD bindable;
old 8-entry pad= lines get RT appended. Right trigger no longer doubles as RB.
Options row 5 "Rules for games here" (rules=tn2|classic), rows compacted to fit 1280x800 with
a status line. Server: --tn2. Local Debug+ASan/UBSan ctest 11/11, python tests OK; core has
test_tn2_rules/game/network + a TN2 bot game; negative control (immunity block and confusion
map disabled) fails 2 checks. Screenshots data/tetrinet/tn2/ (capture-tn2.c).
d84611d passed linux/windows/interop CI 38910716381; tag v0.1.11 release 38912229301 passed.
All eight assets downloaded, SHA256SUMS OK; packaged Linux reports 0.1.11, its server shows
--tn2 and answers --query, 8-player demo smoke ok; AppImage --version 0.1.11.
https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.11
Open for Jan: TN2 play feel (Darkness window, Confusion, mutant shapes) and RT/R3 hold on the
Deck. Follow-ups on ROADMAP: pure mode, ARE, piece frequencies, shuffle. Next release > v0.1.11.

## Earlier: AppImage released as v0.1.10 (item tetrinet-appimage, done)

Opus filler t_mvazpwug3oehjb. tools/linux/build-appimage.sh wraps the finished portable tree
(same as the tar.gz) with pinned appimagetool 1.9.1 + static type2 runtime 20251108 (sha256
checked in release.yml; no libfuse2 needed). AppRun: `python3 -I launcher.py` when python3
exists (fault reports; never updates, the image is not under ~/.local/share), else the binary
with LD_LIBRARY_PATH. Embedded gh-releases-zsync update info + published .zsync (needs zsync
apt package; appimagetool writes the .zsync into the CWD, the script cds to the output dir).
Release CI smokes --version and an 8-player demo via --appimage-extract-and-run.
CI 38906781470 on f40c966, release 38907188291; all eight assets downloaded, SHA256SUMS OK,
published AppImage reports 0.1.10 and renders the demo. Open for Jan: double-click via FUSE
on the Deck and Gear Lever updating (not testable in the container). Windows installer
parity and signing remain on the parent roadmap line. Next release > v0.1.10.

## Earlier: server browser released as v0.1.9 (item tetrinet-server-browser, done)

Opus filler t_mvavfgk41w1iqm. Public servers answer the classic query protocol on the game
port (one connection, several commands): "playerquery" -> "Number of players logged in: N",
"version" -> line + "+OK", "listchan" -> `"name" "desc" players max priority status` lines +
"+OK", all newline-terminated. Checked live 2026-10-16: tetrinet.de (1.13.28a-Sandrini, 31
channels, incl. "Only TetriFast 1.14" ones — a TetriFast candidate), tetrinet.fr (1.13.2ice,
44 channels), tetrinet.us (accepts our login, does not answer queries). tetrinetx 1.13.16
answers too (CI: "1.13.16+qirc-1.40b", one channel #tetrinet); tetrinet-server 0.11 does
not (waits for a login, does not mind the probe).
Core: src/core/tn_query.{h,c} (blocking tn_query, parsers, tn_query_split for host[:port]
and [v6]:port, now also used by connect_online); tn_conn_send_line; tn_server answers
playerquery/version/listchan/listuser before login and keeps the connection open.
`gtetrinet-ng-server --query HOST[:PORT]` (exit 0 answered, 3 reachable only, 1 down).
App: SCR_SERVERS (src/app/browser.c) opened by "Play online": settings server1..server6
(recent, newest first, migrated from host=) + public list (GTN_SERVER_LIST overrides, tests
use it), one detached SDL thread per probe, refcounted so closing never waits. A server is
remembered only once a local client got on it. "Type an address..." = the old SCR_ONLINE;
B there returns to the list. tests/test_browser.c (63 checks: parsers, recent list, real
TCP ours/silent/closed, close-while-asking, keys, connect+remember, en/de 1280x800+800x600
zero cut text). Negative control with the server's query answers disabled: 4 failures.
interop.sh queries all three servers (ours 0, tetrinet-server 3, tetrinetx 0). First CI
38894329876 on 43ac37c: tetrinetx answered (expectation was wrong) and Windows needs ~2 s
to refuse a connect, so the failed-connect check now waits up to 6 s. Local Debug
ctest 11/11, python tests OK, interop (own + tetrinet-server) passed.
252889b passed linux/windows/interop CI 38895013346 (11/11 both OS incl. browser; interop
queried ours/tetrinet-server/tetrinetx as expected). Tag v0.1.9 release 38895395159
succeeded; six assets HTTP 200, SHA256SUMS match (Linux 831686 B, Windows ZIP 7185357 B);
packaged Linux reports 0.1.9, its --query answers for tetrinet.fr, 8-player demo smoke ok.
https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.9
Open: channel choice (/join) belongs to the partyline item; browsing on the Deck is for Jan
to try. Next release > v0.1.9.

Warpzone handoff (request t_mvavxtn02aupkt, from the laptop's warpzone work, not Jan):
descriptor games/gtetrinet-ng.toml in jarvis-jhk/warpzone checked against the v0.1.8
assets — asset names, SHA256SUMS, exec paths (gtetrinet-ng/gtetrinet-ng.sh, .exe) all
match; tier 0 is honest (no --warpzone-join/--warpzone-host in the game yet); no [[data]]
needed; no [steam] URLs because our artwork exists only inside the archive (Warpzone then
generates it). No PR needed today. launcher.py now skips its update check when the
install dir or its parent has .warpzone-managed (test_launcher). Integration itself
(tier 2 rooms via the C API, peer channel, FFA ladder) is a new M2 roadmap item.

Warpzone fault reports (request t_mvc434n79bz3f, warpzone lane, 2026-10-17, not Jan): asked
to route crashes/bugs through Warpzone's feedback API (game id gtetrinet-ng → lane tetrinet).
Not done yet on purpose: the request itself says to wait for libwarpzone 0.1.3, which is not
released (latest v0.1.2) and https://warpzone.virtualtabletop.io/api/v1/feedback answers 404.
Launcher-started crashes are already reported by Warpzone itself. Sub-item under the M2
Warpzone roadmap entry; a work session picks it up once 0.1.3 is out.

## Current: online interop released as v0.1.8 (item tetrinet-online-interop, done)

Opus filler t_mvataavx18bmir. tests/interop.sh CORE [GAME] builds unmodified tetrinetx
1.13.16+qirc-1.40c (sourceforge tarball, sha256 2b7210a0…d15d4, gcc main.c -ladns; needs
HOSTNAME with a dot and /etc/protocols — the agent container lacks the latter, CI has it)
and starts Ubuntu's tetrinet-server 0.11 (HOME=tmp, foreground) and our own server, then
runs tests/test_interop.c (two core clients: login, chat, teams, fields, specials both ways,
won game twice, taken nick → "Nickname…", seventh player refused or seated elsewhere,
leave seen) and tests/test_online.c (the game with keyboard + 3 pads through
connect_online: online "Jan" forces P1 to Jan_9, online player is moderator and starts,
all four play, stop_session, server still alive, new login works).
Findings: (1) neither server accepts TetriFast — both only decode "tetrisstart" and hang
up; the core now says "the server does not accept TetriFast - pick classic with Y" (de
translated). (2) tetrinetx puts a 7th player into the next channel instead of refusing.
(3) tetrinet-server dies of SIGPIPE when several clients hang up at once; stop_session now
leaves non-hosted servers one seat per 60 ms. Negative control without the stagger: server
dead, 2 failures. (4) App bug: the frame a seat retried a taken nick did not count as
pending, so the lobby opened early — fixed. (5) A failed connect no longer drops seats
while nobody is in (p->failed), so a couch keeps its seats after a typo or TetriFast.
Local: interop.sh all passed (2 min; own 45/45/35, church 45/2/38, tetrinetx 45/2/38);
Debug core/i18n/names/bots/sound pass. New CI job `interop` (Ubuntu 22.04, ASan/UBSan).
CI 38888375879 (fc33904) failed only in the interop fixture: the TetriFast run on our own
server started before the server dropped the previous run's Anna (nick taken); d79b462
sleeps 1 s between runs. d79b462 passed linux/windows/interop CI 38888888108 (interop log:
all three servers, every run 0 failures, tetrinetx tarball sha256 OK). Tag v0.1.8 release
38891195293 succeeded; six assets HTTP 200, SHA256SUMS match (Linux 821308 B, Windows ZIP
7174736 B); Linux package reports 0.1.8, contains the new message, 8-player demo smoke ok.
https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.8
Open: legacy GTetrinet client fixture, moderator hand-over, a TetriFast-capable server;
playing on a real public server is for Jan to try. Next release > v0.1.8.

## Current: computer opponents released as v0.1.7 (item tetrinet-bot-opponents, done)

Opus filler t_mvasr12a117hj1. Core: tn_bot_init_level(level, seed) — Easy 420 ms/25 %
mistakes, Normal 210 ms/6 %, Hard 95 ms/0 %; a mistake scores every fitting place at
random (own LCG per bot). App: DEV_BOT seats (gtn_local.bot), named CPU1..CPU8 (first
free), added only when a->hosting and in the lobby: X/S adds, LB/Q removes the last bot,
RB/E cycles strength for seated bots too, saved as bots=easy|normal|hard. update_session
drives each bot while its client is ingame; stop_session deactivates bots; nick retry
uses CPU as base. Lobby shows two hint lines under the slot list when hosting; player
card label "Computer". tests/test_bots.c (42: levels, settings, add/remove/reuse names,
online refusal, keyboard-not-joined, render en/de 800x600+1280x800 zero cut text, real
TCP game where an idle human tops out while two Hard bots lock/clear/use specials and
stay seated back in the lobby, B ends session and bots leave). Negative control without
the bot update: fails (17 locks, 0 clears). Debug ctest 10/10 local.
Source 3bb75ab passed Linux + native Windows CI 38885036861 (10/10 incl. bots on both).
Tag v0.1.7 release workflow 38885394792 succeeded; all six assets downloaded HTTP 200,
SHA256SUMS match (Linux 819934 B, Windows ZIP 7174612 B); Linux package 8-player demo
smoke passed, new lobby strings present in the binary; Wine skipped (VPS rule).
https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.7
Open: bot play feel/strength balance on the Deck (Jan tests releases). Next release > v0.1.7.

## Current: sound effects and rumble released as v0.1.6 (item tetrinet-sound-haptics, done)

Opus filler t_mvarohlmr1h6j. Core: tn_client.ev (tn_events) counts locks, clears
(+last_clear), specials used, hits (+last_hit sb), losses and wins (playerwon for our slot
or our team). App: src/app/sound.{h,c} synthesizes 12 effects at start-up into a bank
(22050 Hz mono S16, triangle/square/pitched-noise notes with decay and end fade) and mixes
up to 12 voices in an SDL audio callback; no device = silent, never a fault.
gtn_sound_play dedupes one effect per frame (gtn_sound_frame runs at the start of
update_session), so eight simultaneous locks are one sound. main.c feedback() diffs
p->seen against client.ev each frame; a counter going backwards = new session, resync.
Rumble (SDL_GameControllerRumble, DEV_PAD only): hurt 0.75/0.35 220 ms, help 0/0.35 90 ms,
mixed 0.45 160 ms, four lines 0.2/0.5 150 ms, loss 0.9 450 ms, win 0.3/0.6 300 ms.
Settings sound=off|quiet|normal|loud (default normal), rumble=on|off; malformed = default.
Options rows 3 Sound effects (preview plays CLEAR2) and 4 Controller rumble (switching
rumbles the pressing controller). Options layout tightened to fit five rows at 1280x800.
Checks: tests/test_sound.c (133: effect lengths/peaks/no clicks/determinism, settings,
options flow, en/de renders with zero cut text at 1280x800, real two-pad TCP game: lock
dedupe, four-line clear + classic add-to-all hit, harmful/helpful/mixed specials with
rumble on the receiving pad only, rumble off, top-out + win, no burst on a new session,
dummy audio driver mixing). Negative control without feedback(): 16 failures.
test_controls adapted (Up from row 0 now wraps to rumble). Debug 9/9 CTests local.
Source 470d5c0 passed Linux ASan/UBSan + native Windows CI 38881750817 (9/9 incl. sound on
both). Tag v0.1.6 on b1b1bdf (changelog only); release workflow 38882086606 succeeded; all
six assets downloaded HTTP 200, SHA256SUMS match (Linux 815630 B, Windows ZIP 7171914 B);
Linux package 8-player demo smoke passed with the dummy audio driver; Wine smoke skipped
(VPS rule). https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.6
Open: actual sound/rumble feel on the Deck and real pads (Jan tests releases); bot
opponents remain the other half of the M1 line. Next release must exceed v0.1.6.

## Current: nickname editing per seat released as v0.1.5

Work session t_mvaqgvr2dvbaa, combined-roadmap item tetrinet-nickname-editing.
Title screen: a joined player presses Y (keyboard N, mapped to BTN_Y outside games only)
and gets SCR_NAME: on-screen keyboard gtn_name_rows (digits, letters, _ . - !, Aa/DEL/OK;
Y toggles capitals, X deletes, Start saves, B cancels); typed text goes to the entry too.
Only the named player's device or the keyboard acts there; anyone's B closes it when the
named player's controller is gone. gtn_nick_clean: one word of printable ASCII, max 15.
Empty and names already used by another local player are refused with a status line.
Saved as nick= (P1, also the base) and nick2..nick8 (settings.seat_nick[i]); seats
without their own name keep following P1's name + number, and are recomputed when P1
renames. Online/local OSK navigation is now one helper (osk_button / draw_osk).
Checks: tests/test_names.c (61: cleaning, malformed seat keys, full pad/keyboard flow,
renders en/de at 800x600 and 1280x800 with zero cut text, real local TCP session where
all three clients see Jan/Mo/Jan3). Locally only test_names (Debug) and the Python
string test were run (VPS rule since 2026-10-16: heavy builds go to CI or the re lane);
the sanitizer matrix and Windows run in GitHub CI.
Source e9f7df2 passed Linux ASan/UBSan + native Windows CI 38877632541 (8/8 tests incl.
names on both). Release workflow 38878031791 succeeded; all six assets downloaded HTTP 200
with matching SHA256SUMS (Linux 808999 B, Windows ZIP 7165372 B); Linux package smoke
(8-player demo) passed; Wine smoke skipped under the VPS rule.
https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.5
Open: physical controllers/Deck typing feel unverified. Next release must exceed v0.1.5.

## Previous: rearrangeable controller buttons released as v0.1.4

Work session t_mv8w5tlh0kirs, combined-roadmap item tetrinet-controller-remapping (done).
Options gains a third row, Controller buttons (Standard/Custom; Y resets), opening
SCR_CONTROLS: eight in-game actions (rotate CW/CCW, hard drop, special on target/self,
previous/next target, discard) over the eight bindable buttons Up/A/B/X/Y/LB/RB/Back.
The layout is always a permutation: binding a taken button swaps (gtn_pad_bind), so no
action is lost. Saved as pad=A,B,UP,X,Y,LB,RB,BACK in action order; anything not a
complete permutation keeps the default, and a zeroed/invalid map translates as default.
Applied only in game_button and only for DEV_PAD (gtn_pad_translate maps the pressed
button to the default button of its action); keyboard, menus, Left/Right/Down, Start and
Back+Start stay fixed. Capture waits for a controller button; Start, any key or an
unbindable button cancels. enum gtn_btn moved from app.h to settings.h.
Checks: tests/test_controls.c (98: parsing incl. ten malformed files, swap semantics,
options/controls flow before joining, en/de renders 800x600/1280x800 with zero cut text
at 1280x800, real two-client TCP game: swapped rotation, Up/stick no longer drops, Y
drops, keyboard unchanged, Back discards). Debug and ASan/UBSan 7/7 CTests, 7 Python
tests. Negative control without translation fails 3 intended assertions.
Evidence data/tetrinet/remap/. Source 04a1e8d passed Linux + native Windows CI 38669264156
(controls test ran on both). Release workflow 38669529060 succeeded; all six assets
downloaded HTTP 200 with matching SHA256SUMS (Linux 806374 B, Windows ZIP 7161759 B),
smoke-release.py passed. https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.4
Open in remapping: per-player layouts, keyboard remapping; physical controllers unverified.
Next release must exceed v0.1.4. No CI continuation; work schedule db47dddd stays.

## Previous: German translation released as v0.1.3

Work session t_mv61a1bb0y8rg, combined-roadmap item tetrinet-german-translation.
New src/app/i18n.{h,c}: English strings are keys, `gtn_tr()` looks up German; `N_()`
marks stored literals. The core stays language-free: its chat/attack log lines and its
own error texts are translated at display time by templates (`gtn_tr_log`). Chat,
server text, team names (wire data) and OS error strings pass through unchanged.
Font: font8x8_ext_latin (public domain) adds U+00A0..U+00FF; text is decoded as UTF-8,
invalid bytes as Latin-1 (classic clients' nicknames), typographic dashes/quotes map to
ASCII. Width/fit count characters and never cut inside a multibyte sequence.
Options (X/S on the title) now has two rows: Movement repeat and Language
(Automatic via SDL_GetPreferredLocales, English, Deutsch), saved as lang=auto|en|de;
`--lang` overrides for one run. Changelog reflows paragraphs; a German note says the
entries are English. Fixed on the way: empty title cards overflowed with long text;
the lobby winner list overlapped the start prompt with eight players.

Checks: new tests/test_i18n.c (4175 checks: table placeholders, UTF-8/Latin-1 decoding,
glyph pixels for ÄÖÜäöüß, whole-character cutting, German log templates, the options
page by controller and saved/malformed lang values, and every screen drawn at 1280×800
in both languages with zero cut texts — this caught ZEILEN/STUFE overflowing eight-player
panels, now STF/ZLN, and an uninitialised index in the attack-line parser under ASan).
tests/test_i18n_strings.py: every marked literal has a German entry, no stale entries,
no unmarked English phrase in ui.c/input.c. Debug ASan/UBSan: all six CTests pass;
seven Python tests pass. MinGW cross-build: test_i18n.exe 4175 and test_repeat.exe 97
checks pass under Wine/Xvfb. German eight-player demo smoke runs (dummy video).
Evidence and screenshots: data/tetrinet/german-translation/. Screens were viewed.
Open: German installer/launcher dialogs; physical Deck legibility of the German UI.

Released v0.1.3: https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.3
Source 2ff0469 passed Linux sanitizer + native Windows CI 38335776837 (i18n test ran on
both). Release workflow 38335957110 (Linux, Windows, publish) succeeded. All six assets
downloaded HTTP 200 with matching sizes and SHA256SUMS (Linux 802594 B, Windows ZIP
7158068 B); smoke-release.py passed and the extracted Linux build runs a German eight-player
demo. Checkpoint data/tetrinet/verified-v0.1.3.json. Next release must exceed v0.1.3.
No CI continuation needed; regular work schedule db47dddd stays.

## Previous: saved movement-repeat presets locally verified

One bounded Sol slice (2026-10-12, t_mv4xmp046jk04): title-screen controller X
or keyboard S opens Movement repeat, including before anyone joins. All local
keyboards/controllers share saved Normal170/50 ms, Relaxed250/100 ms or Fast120/30 ms
initial-delay/repeat-interval presets. D-pad/arrows/A change, Y resets Normal,
B/Esc/Start returns. Existing files and malformed values default to Normal.
Physical remapping, nicknames, accessible typography and eight-controller Deck
acceptance remain open. No core/wire changes or release/tag in this slice.

Preference writes now detect open/write/close/rename failures and forward real
faults through the existing anonymous relay without sending settings or paths.
The options page explains when a choice could not be saved but works this run.
POSIX replacement uses rename without first removing the previous settings file;
Windows retains its existing remove-before-rename convention. The tests use an
isolated preference directory, never the platform user's actual settings.

New `repeat` CTest drives actual keyboard/controller events and two embedded-server
TCP clients. Its93 checks verify preset persistence, preservation of host/nick/mode/
version, no accidental joins, release handling, wrap/reset/back, exact initial and
subsequent repeat boundaries, device isolation, reversal and multiple repeats in
one update, old/malformed settings, real failed-save notification/recovery and
800x600/1280x800 options rendering. Both screenshots were visually inspected.
Debug and non-PIE ASan/UBSan pass all five CTests (core1458, input76, virtual1034,
effects49462, repeat93); sanitizer CTest uses leak detection. An independently
compiled negative control with the previous fixed-timing input.c fails19 executed
repeat assertions. Tracked sources were never mutated for that probe.
Eight-player sanitizer SDL/Xvfb smoke passes120 frames (render leak detection off,
as in existing CI). MinGW Debug cross-compilation also passes; the resulting
Windows repeat test passes93 checks under Wine/Xvfb. This is separate from native
Windows CI. Actual agent TypeScript gate passes before commit.
Evidence: data/tetrinet/repeat-presets/ (logs, screenshots, source hashes).

Native Windows CI failed the repeat test on 5df2db9, 2c4ae2b and be565a3. The real cause,
found with a diagnostic branch (ci/repeat-diag, runs 38220519520..38222118414): the test's
`SDL_Event e = {0}` initialises only the union's first member, and native MinGW left
`key.repeat` = 127, so the app correctly ignored every synthetic keypress as an OS
auto-repeat. Pads were unaffected. Linux/Wine happened to zero it. Tests now use SDL_zero.
The two moderator changes made on the wrong theory stay: combining every local client's
slot view and refusing start while a local seat is still joining are both correct, with
a deterministic check. Fix head 2dac353 passed Linux and native Windows CI 38222383588.

The preceding effect-color source ba4cf08 passed both Linux sanitizer and native
Windows CI: https://github.com/jarvis-jhk/gtetrinet-ng/actions/runs/38209642858
Existing CI automatically includes the new repeat test. Inspect Linux/native Windows
results for the pushed head before releasing. No new CI continuation or schedule;
latest published release is v0.1.2 and the next tag must exceed it.

## Previous slice: special effect colors and target cues locally verified

One bounded Sol slice (2026-10-12, t_mv4tpblfnfwu9): all nine special tiles keep
their classic letters with recipient-relative colors. C/N/G are green helpful,
A is red harmful, and R/S/B/Q/O are amber situational (random holes, field swaps,
collectibles/bombs, row shifts and debris). The next-special line adds +HELP/-HURT/?MIX
and its full name; TARGET adds +You/+Them, -You/-Them, ?MIX or ?Both/?Self.
README explains teammate/opponent/self targeting and the switch safety rule.
No core rules, wire ids or input behavior change. Ordinary block colors remain
distinct from the special palette. Small tiles below eight pixels cannot fit a
classic letter; the separate next-special label still supplies text and symbols.

New `effects` CTest checks all nine rendered letter tiles at ten sizes, category
mapping, recipient hints, and actual full-queue one/eight-player layouts at
1280x800 and 800x600. Fixed pixel references assert every queue label and all eight
target hints in both self/other cases. Debug and non-PIE ASan/UBSan builds with
at most two build jobs pass all four CTests: core1458, input76, virtual1034 and
effects49462 checks, zero failures. Leak detection is enabled in sanitizer CTest.
A separately compiled negative control restoring the old unnamed-category queue
label fails15564 executed pixel checks; tracked sources were never mutated for
that probe. Sanitizer SDL/Xvfb eight-player smoke passes120 frames (render-smoke
leak detection disabled as in existing CI). Actual agent TypeScript gate passes.
Screenshot fixtures are visually inspected, separate from Deck hardware proof.
Evidence: data/tetrinet/effect-colors/ (logs, reference screenshots and hashes).

The previous virtual-controller head7583f7d passed Linux sanitizer and native Windows:
https://github.com/jarvis-jhk/gtetrinet-ng/actions/runs/38207937807
That effect-color push passed both CI jobs in38209642858 (verified2026-10-12);
its original local checkpoint is retained above. No release/tag or CI continuation is
created by this budget tick. Latest published release remains v0.1.2; next tag
must exceed it. Physical eight-controller Deck verification remains open.

## Previous slice: M1 SDL virtual-controller pipeline locally verified

One bounded Sol slice (2026-10-12): new `input_virtual` CTest uses real SDL virtual
devices, never injected controller events. Eight controllers own eight distinct TCP
client slots in an embedded match; a ninth cannot join. Every seat's movement and
both rotations remain isolated, concurrent horizontal repeat/soft-drop holds release
independently, and a middle seat can leave/rejoin four times without stale state.
Four held-stick/soft-drop/Back disconnect cycles retain the player/nickname/network
slot, clear input state, prevent repeat after unplugging and accept fresh input after
takeover by an existing spare or newly attached replacement. Twelve SDL adds/removals,
140 button events and 24 axis events are observed by the actual application dispatcher.
All controller references are closed and all virtual devices detached at completion.

Debug and non-PIE ASan/UBSan builds (`--parallel 2`) pass all three CTests:
core1458, simulated input76 and virtual controller1034 checks. Leak detection is
enabled for CTest; sanitizer eight-player SDL/Xvfb screenshot smoke also passes
(120 frames, leak detection disabled for the rendering smoke, as in existing CI).
A negative control retaining the disconnected instance id fails the executed
takeover assertion; original input.c was restored byte-for-byte, rebuilt and all
Debug tests passed again. No gameplay source change was needed. Actual agent
TypeScript compiler gate passed before commit.

SDL minimum is now2.0.14 for the virtual-device API. Existing Linux sanitizer and
native Windows workflows run the new CTest automatically; inspect their results
on the pushed source head before any release. No release/tag or CI continuation
is created by this budget tick. Physical eight-controller Deck, suspend/resume,
Steam Input and docking verification remain open. Local evidence is under
data/tetrinet/virtual-controller-tests/. Latest published release remains v0.1.2;
the next release tag must exceed it.

## Latest release: Deck feedback v0.1.2 verified

https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.2

Annotated tag/source18308eef9ea61f88ad7c707bc1a4478955a6689c.
Exact-head Linux sanitizer and native Windows CI passed:
https://github.com/jarvis-jhk/gtetrinet-ng/actions/runs/38187173734
Release Linux/Windows packaged smoke and publication passed:
https://github.com/jarvis-jhk/gtetrinet-ng/actions/runs/38187315858

All six public downloads returned HTTP200, matching sizes and SHA256SUMS: Linux
archive791090 bytes, Windows ZIP7147485, install.sh2826, launcher.py5122,
Install-GTetrinet-NG.desktop674 and SHA256SUMS447. Downloaded desktop passes standards
validation, and five original Steam images are present. Public Linux eight-player and
Windows ZIP Wine/Xvfb launches pass. Downloaded bootstrap installs the exact public
archive with real hashes; repeated current-version check and managed one-player launch
pass. Evidence: data/tetrinet/verified-v0.1.2.json. No CI continuation remains.

Request t_mv487i6v4p7fol: Jan reports one-player v0.1.1 on his physical Steam Deck
works. Eight physical controllers remain unverified. German UI translation, effect
colors and a TetriNET 2 parity audit added to ROADMAP. Clockwise/counterclockwise
already exist; explicit instructions and real live-session button assertions added.

Clickable release-bound desktop installer now uses dialogs, keeps the atomic updater,
adds the friendly GTetrinet NG — TetriNET title and original Steam artwork rendered by
our own game. Existing shortcut appid survives title migration, duplicate installs are
idempotent and live Steam still blocks VDF writes. New installer failures use the same
anonymous public ntfy relay (no private endpoint or diagnostics in public source).

Text scaling, shortened context help and changelog heading wrapping fix fixed-font
clipping. Special effect labels have their own line below the queue, so a full queue
cannot hide them. Layout/artwork evidence is in data/tetrinet/artwork-preview, separate
from physical hardware proof. Local Debug/ASan/UBSan core1458 and input76 checks pass; five Python checks pass
(atomic install, bad hash/path rejection, VDF title/appid/artwork preservation, and
repeated graphical shell installation with spaces in HOME). Screenshot/artwork smoke
passes; transparent logo alpha is verified. Desktop entry passes freedesktop validation and GIO
parses its command exactly; the repeated-install test executes the desktop command itself. Agent TypeScript gate passed. Release gates completed above.
First implementation39d3b0c passed Linux and Windows in CI38171723312. Final desktop
quoting fix87c1a47 adds freedesktop validation to both CI and release packaging.

## First public preview v0.1.1 verified

https://github.com/jarvis-jhk/gtetrinet-ng/releases/tag/v0.1.1

Annotated tag at 3a2b001a8939eeec89f8cdc0aec5e9c9bafc63f6.
Linux sanitizer and native Windows CI both passed:
https://github.com/jarvis-jhk/gtetrinet-ng/actions/runs/38157018234
Release build, both packaged smoke tests and publication passed:
https://github.com/jarvis-jhk/gtetrinet-ng/actions/runs/38157273111

All five public assets downloaded with HTTP 200, matching metadata sizes and SHA256SUMS:
Linux tarball 757661 bytes, Windows ZIP 7146663 bytes, installer 1183 bytes,
bootstrap launcher 5122 bytes, checksums 352 bytes. Extracted Linux launcher and Windows
ZIP (Wine/Xvfb) run eight-player demo matches. Published bootstrap installs/checks hashes
and launches from an isolated managed install; current-version update check succeeds.
Native Windows packaging smoke excludes the toolchain DLL directory from PATH.

## Implementation and evidence

Kickoff resumed from a Claude quota handover: retained full upstream history and GTK2
source in legacy/, completed C11/SDL2 core, frontend and offline/LAN server. Ownership
brief, architecture, roadmap, README, packaging, Linux atomic updater/Steam shortcut,
Windows launcher and GitHub CI/releases are now in place.

Local Debug: core 1458 checks, simulated SDL routing 66 checks; installer 3 tests pass.
Eight distinct application client sessions start a real TCP match. Routing tests uncovered
stuck soft-drop/Back state after disconnect, now fixed. Steam VDF fixture verifies stable
appid, exact executable matching and name/icon changes. Docs screenshot is a running
1280×800 demo, not hardware. Agent TypeScript gate passed before every code commit.
Local non-PIE ASan/UBSan core/input and eight-player smoke pass. PIE sanitizer binaries
intermittently failed without diagnostics in this container; normal PIE GitHub CI passed.

Anonymous fault relay: central fault helper tested with a stub curl (no synthetic report
posted, diagnostics omitted). Collector dispatch 38156653706 passed on an empty topic.
Public ntfy → GitHub issues, private queue endpoint only in JARVIS_REPORT_URL repo secret.
Issue #1 (2026-10-12, six "0.1.0 Linux application fault" posts 07:49–08:11 UTC) was our own
maintainer smoke runs: local build dirs still cache GTN_VERSION=0.1.0 (never published) and
ran the UI without GTN_NO_REPORT. Game fault posting is now opt-in via GTN_FAULT_REPORTS
(OFF by default, ON only in release.yml); stub-curl check: dev 0 posts, release 1, opt-out 0.
Windows bundles SDL's libiconv/charset dependencies, licences and matching source archive.
Linux bundles SDL2 and samplerate when needed; host audio codecs remain with host libraries.

## Remaining acceptance work

M1: physical eight-controller Deck session,
suspend/resume, Game Mode, dock/undock and legibility after the text fixes. Simulated button events and bot
demos are separate evidence. Third-party servers are covered by tests/interop.sh (see the
online-interop entry); a legacy-client fixture, malformed input fuzzing, asynchronous DNS,
Windows updater and AppImage remain open. Direct EXE launch bypasses
launcher crash forwarding; standalone server is not hardened for public hosting.

## History

Unpublished v0.1.0 tag at 50f6658 remains unchanged. Release build 38156887765 failed
because Ubuntu SDL did not need samplerate and its copyright package was absent;
Windows packaged smoke passed. Conditional notice packaging fixed this in v0.1.1.
Do not reuse either existing tag. Next release must exceed v0.1.1.
No CI continuation is needed. Regular work schedule db47dddd remains the next work session.
