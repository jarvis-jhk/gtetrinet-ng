# Roadmap

Jan edits goals; JARVIS owns delivery. Checkboxes mean implemented and locally verified,
not certified hardware compatibility. Release gates and remaining checks are in NOTES.md.

## M0 — first modern, installable build (0.1.0 preview)

- [x] Preserve original GTK2/GNOME2/ESD source under legacy/ and full history.
- [x] C11 portable core, SDL2 rendering/gamepads, Linux and Windows builds.
- [x] Embedded offline server, standalone LAN server, keyboard play.
- [x] Game rules, specials, teams, win list, protocol and real TCP integration tests.
- [ ] Linux/Windows CI, published portable assets and verified download.
- [x] Steam Deck installer, Steam shortcut, update checks and in-game changelog.
- [x] Anonymous fault relay via ntfy, GitHub issues and private queue secret.

## M1 — eight controllers on the Deck

- [x] Eight local client slots and responsive board layout; hotplug/takeover input.
- [ ] SDL virtual-device tests (simulated event tests already cover routing): independent ownership, join/leave,
  held-input disconnect, reconnection, device replacement and repeated slot reuse.
- [ ] Physical Steam Deck + eight-controller play session; suspend/resume, Game Mode,
  dock/undock and 1280×800 legibility. No hardware certification until this passes.
- [ ] Controller remapping, repeat settings, nickname editing and accessible typography.
- [ ] Sound, haptics and offline bot opponents selectable in the lobby.

## M2 — online interoperability

- [ ] Independent tetrinetx/legacy-client fixture: login, fields, specials, teams,
  classic and TetriFast timing, moderator changes and six-slot matches.
- [ ] Server browser, partyline/chat UI, profiles and reconnect policy.
- [ ] Malformed-message fuzzing and limits for public-facing hosting.
- [ ] IPv6 and asynchronous DNS; avoid blocking the render loop on lookup.

## M3 — distribution polish

- [ ] AppImage, Windows installer/update parity, signed releases where practical.
- [ ] Optional crash-report consent UI and local diagnostic export (redacted).
- [ ] Localization and original upstream theme migration.
