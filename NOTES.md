# Running state — 2026-10-11

Kickoff resumed from a Claude quota handover. Inherited work: preserved upstream history,
legacy/ move, modern C/SDL2 core/frontend/server, bot demo/tests and initial Linux/MinGW builds.
Completed this session: ownership brief, staged roadmap, README/architecture, packaging,
Linux atomic update installer/Steam shortcut, Windows launcher, release/CI workflows and
anonymous fault relay. GitHub JARVIS_REPORT_URL secret configured; endpoint not in this repo.

Local Debug core and simulated SDL input tests pass. Eight distinct application client
sessions start a real TCP match; routing/replacement tests uncovered stuck soft-drop and
Back state after disconnect, now fixed. Installer tests reject bad hashes/path traversal and
preserve existing installation. Screenshot in docs/ is a running 1280×800 demo, not hardware.
Linux dummy-driver SDL smoke passes. MinGW Windows build and Wine execution pass: core
1458 checks, input 66 checks. Agent TypeScript gate and Python installer 3 tests pass.
ASan/UBSan core passes directly and under gdb, but intermittently segfaults in this
container before producing a diagnostic; investigating before relying on CI. Publication
remains gated on CI and actual release-asset verification.

Known limitations: hardware/virtual-device tests remain open, online interoperability is
self-tested only, synchronous DNS, no sound, font/localization/remapping pending. Windows
auto-update and AppImage are follow-ups. Linux startup updates retain the previous install
on network failure; only managed installs update automatically. Standalone server is a
preview, not hardened for untrusted public hosting. Windows can bypass launcher by using
EXE directly; crash forwarding then depends on launcher for abnormal termination.

Next: finish Linux/Windows CI for this head, tag 0.1.0, verify archives/installer publicly,
then announce once with release URL. M1 should begin with actual virtual joystick tests,
then Jan's hardware feedback when he tries the release. Do not present demo bots as input proof.
