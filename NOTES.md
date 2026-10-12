# Running state — 2026-10-12

## Current: saved movement-repeat presets locally verified

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

Native Windows CI 38219371782 (5df2db9, rerun) failed the repeat test: the lobby Enter
start was ignored. Cause was a product race, not timing: a local client in slot 2 that had
not yet seen slot 1 join believed it moderated, sent startgame, and the server ignored it.
local_moderator now combines every local client's slot view. Deterministic lagging-view
check added (negative control with old main.c fails it); Debug ASan/UBSan five CTests pass
(repeat95). Linux CI on 5df2db9 passed. Inspect native Windows CI for the fix head.
Windows CI 38219921084 on 2c4ae2b still failed: the single early Enter found no moderator
(or a lagging one) and was dropped; the test never pressed again. local_moderator now
returns none while any local seat is still connecting/logging in (status hint shown), and
the test waits for a known moderator before Enter, as a player would. Debug ASan/UBSan
five CTests pass. Inspect native Windows CI for this head before release.

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
