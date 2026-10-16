# Running state — 2026-10-16

## Current: nickname editing per seat (e9f7df2), release v0.1.5 pending CI

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
Open: physical controllers/Deck typing feel unverified.

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
demos are separate evidence. Online interoperability is self-tested only; independent
legacy/tetrinetx fixtures, malformed input fuzzing, asynchronous DNS, sound, remapping,
localization, Windows updater and AppImage remain open. Direct EXE launch bypasses
launcher crash forwarding; standalone server is not hardened for public hosting.

## History

Unpublished v0.1.0 tag at 50f6658 remains unchanged. Release build 38156887765 failed
because Ubuntu SDL did not need samplerate and its copyright package was absent;
Windows packaged smoke passed. Conditional notice packaging fixed this in v0.1.1.
Do not reuse either existing tag. Next release must exceed v0.1.1.
No CI continuation is needed. Regular work schedule db47dddd remains the next work session.
