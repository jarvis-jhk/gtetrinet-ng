# gtetrinet-ng maintenance brief

This public GPL-2.0-or-later continuation preserves GTetrinet's full upstream history.
JARVIS owns implementation, documentation, issues and releases. Jan supplies roadmap goals.
Commit as jarvis-jhk <326430845+jarvis-jhk@users.noreply.github.com>.
Never publish the owner's personal domains, credentials, report endpoint, device data or logs.
The private report endpoint belongs ONLY in the GitHub JARVIS_REPORT_URL secret.

Primary goal: up to eight independent controllers playing together on a Steam Deck screen.
Also ship Linux x86_64 and Windows x86_64 releases and retain online TetriNET compatibility.
Read ROADMAP.md and NOTES.md first. Keep current limitations honest; a scripted demo is
not physical controller or Steam Deck hardware verification. Preserve legacy/ and attribution.

Architecture: C11 rules/protocol/network library, SDL2 UI/input, one client/nickname per
local player; embedded server for offline games and standalone server for LAN. Classic
TetriNET servers support six slots. Seven/eight are an extension between our clients;
never claim existing six-slot clients support that extension. Do not silently replace the
wire protocol. See docs/ARCHITECTURE.md.

Build/test the changed targets (Debug + sanitizers for core changes), smoke the SDL UI,
and require Linux/Windows CI before tagging. Release tags vX.Y.Z build portable archives.
Check the public downloaded assets before announcing a release; releases-only Signal.
Keep roadmap and notes current, retain a single scheduled CI continuation if needed.
No release claim for unverified hardware. Do not modify JARVIS code for game changes.
