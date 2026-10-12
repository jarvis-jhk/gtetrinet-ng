# Running state — 2026-10-11

## Current: Deck feedback v0.1.2 awaiting release gates

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
parses its command exactly; the repeated-install test executes the desktop command itself. Agent TypeScript gate passes. Exact-head
Linux/Windows CI and published-asset verification remain the release gates.
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

M1: actual SDL virtual-device/hotplug tests, physical eight-controller Deck session,
suspend/resume, Game Mode, dock/undock and legibility. Simulated button events and bot
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
