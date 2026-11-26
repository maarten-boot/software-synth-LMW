# Conversation 2026-11-26

- (Phase 33 from 2026-11-25 still uncommitted: stereo and pan, waiting for the user's look and listening.)
- User reviewed: the lin / log / ± text and the tick lines on the faders, and the coloured socket rings, should be optional (default on, for beginners); the hover texts stay (typos: reviewd, "texta dn", assising). Spec S6.20 proposed (to confirm): Menu ▸ Show fader scales, Show socket rings, config.ini, layout unchanged, /view; Plan Phase 34, Tests planned.
- User asked: a VCO preset saved by name — selectable on the other VCOs too? Yes: module presets are stored per kind (one folder for VCO 1–3, VCF 1–2, ENV 1–2); LFOs and single modules have no presets.
- User: Menu ok, keep the socket size; commit 33 and start 34. User: pan seems not to work (Clarinet, all effects Default, moving Pan: no movement). Checked: the engine test and a new render check through the VST3 (host test: Pan -1 → left only, +1 → right only) pass; the processor gives up panning only when the host / device gives a mono output (JUCE's player sets the bus to mono when the device has one active output channel). Asked the user for the standalone audio settings; adding a hint on Pan / Spread when the output is mono.
- Committed 9b7f858 (Phase 33). Mono hint on Pan / Spread hover. 34 done: Menu ▸ Show fader scales / Show socket rings (config.ini show_scales, socket_rings; layout and socket size unchanged; hover texts kept), /view scales / rings, pictures.sh sets them on; tests; Xvfb checked. 40 of 40.
- User: commit and do the crosscheck; pan confirmed: the headset was set to mono, in stereo it works (T-S5.25 listening ok).

## Crosscheck after Phases 33–34

- Committed e684981. Code vs S5.25, S6.20: as built (noted there). Stale lines fixed: S5a.1 (out.pan, out.spread, out.spreadmode; 79), S9.2 (79 / 2160 host parameters, measured), S6.6 (OUT In | Pan), S6.8 (36 rows), S6.16 (minimum 1594 px), S7.10 (83 terms), S7.12 (pictures 1594 wide). Plan header. 40 of 40. Open: CI after the push.
