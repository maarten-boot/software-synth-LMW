# Conversation 2026-11-26

- (Phase 33 from 2026-11-25 still uncommitted: stereo and pan, waiting for the user's look and listening.)
- User reviewed: the lin / log / ± text and the tick lines on the faders, and the coloured socket rings, should be optional (default on, for beginners); the hover texts stay (typos: reviewd, "texta dn", assising). Spec S6.20 proposed (to confirm): Menu ▸ Show fader scales, Show socket rings, config.ini, layout unchanged, /view; Plan Phase 34, Tests planned.
- User asked: a VCO preset saved by name — selectable on the other VCOs too? Yes: module presets are stored per kind (one folder for VCO 1–3, VCF 1–2, ENV 1–2); LFOs and single modules have no presets.
- User: Menu ok, keep the socket size; commit 33 and start 34. User: pan seems not to work (Clarinet, all effects Default, moving Pan: no movement). Checked: the engine test and a new render check through the VST3 (host test: Pan -1 → left only, +1 → right only) pass; the processor gives up panning only when the host / device gives a mono output (JUCE's player sets the bus to mono when the device has one active output channel). Asked the user for the standalone audio settings; adding a hint on Pan / Spread when the output is mono.
- Committed 9b7f858 (Phase 33). Mono hint on Pan / Spread hover. 34 done: Menu ▸ Show fader scales / Show socket rings (config.ini show_scales, socket_rings; layout and socket size unchanged; hover texts kept), /view scales / rings, pictures.sh sets them on; tests; Xvfb checked. 40 of 40.
- User: commit and do the crosscheck; pan confirmed: the headset was set to mono, in stereo it works (T-S5.25 listening ok).

## Crosscheck after Phases 33–34

- Committed e684981. Code vs S5.25, S6.20: as built (noted there). Stale lines fixed: S5a.1 (out.pan, out.spread, out.spreadmode; 79), S9.2 (79 / 2160 host parameters, measured), S6.6 (OUT In | Pan), S6.8 (36 rows), S6.16 (minimum 1594 px), S7.10 (83 terms), S7.12 (pictures 1594 wide). Plan header. 40 of 40. Open: CI after the push.
- User: postpone the next release; what is next? Listed the open items; user: start the spec for L26; thinking of an extra mixer after the filters; reverb feedback: Cathedral saturates easily, slightly metallic ring in the highs, Damping does little (a flute: few highs) (typos: chatedral, sightly, oerhaps). Answered: Freeverb's fixed wet level builds up at large Size and nothing limited after the effects; the ring is Freeverb's combs; Damping is a mild lowpass inside them. Proposed: (1) MAIN limiter + level compensated for Size, (2) a stronger Damping, (3) a new engine later; for L26 a post-filter mixer with pan per input (stereo from there).
- User: reverb fixes 1 and 2 now, a new engine on the list (L27); the extra mixer after the filters is fine for pan (re-evaluate later; typo: revaluate); 3 inputs; the smallest workable screen stays 1920 × 1080; panning (scrolling) the screen comes soon (L28).
- 35 done: MainLimiter on MAIN, wet compensation by Size (theory +6 dB, measured +3 dB: fourth root of the energy ratio; result −0.1 dB at 95 %), Damping lowpass 20 kHz → 1.2 kHz on the wet signal, Cathedral 85 % / 50 %; host test and test_engine. 40 of 40.
- Spec S5.26 MIX 2 proposed (to confirm): after FORMANT, 3 inputs with Level and Pan, stereo voice from there through the VCA to OUT, VCO spread by patching, default rack row 1 gets FORMANT (window about 1834 px wide), a "Choral flute" preset; Plan Phase 36.
- CI run 45788970152 (fa48ad7) green on all four.
- User: commit; FORMANT in row 1 ok, MIX 2 ok, add Choral flute; perhaps now add more native non-western instrument presets (Plan L13).
- Committed 0d0ee20 (Phase 35). 36 done: MIX 2 (3 inputs, level + pan, stereo voice through the VCA and OUT), FORMANT in row 1, Schematic block, Choral flute, Terms MIX 2, Lesson 9, AI guide; tests (test_pan, counts); Xvfb on 1920 × 1080. 40 of 40.
- Spec S7.13 world instruments proposed (to confirm): Presets ▸ World with 13 presets (Shakuhachi, Bansuri, Pan flute, Duduk, Erhu, Shamisen, Koto, Sitar, Oud, Kalimba, Gamelan, Steel pan, Didgeridoo), limits noted (resonators L12). Plan Phase 37.
- User: commit 36; World submenu ok, start 37; idea for later: keyboards spread by octave, low left, high right (as the natural sound). Answered: that is OUT › Spread, mode Keys (Piano, Harpsichord, Marimba at 40 %); offered it for Organ and Electric piano too.
- User: add the keys spread to Organ and the E-piano, finish 37; the Schematic text size becomes a Plan item (L29). Answered the measurement: Schematic about 11.2 px per unit at 1920 × 1080, labels about 19 px vs the Synth tab's 15 / 12 px: 22 % smaller at 15 px, 37 % at 12 px; compact already about 14 px.
- 37 done: 13 World presets (Instrument::group, detail::like), Presets ▸ World submenu, Organ / E-piano Spread 40 % Keys, AI guide; tests; Xvfb (submenu, Gamelan). 40 of 40.
- User: commit and do the crosscheck.

## Crosscheck after Phases 35–37

- Committed a430c04. Code vs S7.4 (reverb fixes), S5.26, S7.13: as built (noted there). Stale lines fixed: S5a.1 (mix2.*, 85), S9.2 (85 / 2166 host parameters, measured), S6.5 (default rows with FORMANT in row 1 and MIX 2), S6.8 (39 rows, 23 columns), S6.16 (1834 px), S7.7 (33 instruments), S7.10 (84 terms), S7.12 (pictures 1834 wide). Plan header. 40 of 40. Open: listening (Choral flute, World presets, reverb), CI after the push.
- User pushed 443665f (CI run 45810993062 watched); what is next → listed; user: start the spec for L28 and L29. Proposed S6.21 panning the view (bars, wheel, drag on empty space, focus follows, edge scroll; window down to 1280 × 720, up to 6 rows / 30 columns; /view scroll) and S6.22 Schematic at 15 / 13 px text with zoom steps; Plan 38, 39.
- User: all ok (pan by bars, wheel and drag, Home / End; 1280 × 720, 6 rows / 30 columns; Schematic zoom steps), commit the spec and start 38.
- CI run 45810993062 (443665f): macOS and Windows red in the host test's window check (size 1600 × 900 refused: since MIX 2 the smallest editor was 1834 wide; Linux CI has no display so it ran only there). Mended by Phase 38's smaller minimum.
- 38 done: RackArea in a Viewport, all rack components its children, rack coordinates, scroll bars / wheel / drag / Home / End / focus / autoScroll, window down to 1280 × 720, 6 rows, header short texts below 1520 px, /view scroll. Found: JUCE's Viewport took Home / End (vertical only) → a viewport that passes keys on. 40 of 40; host test with a display passes.
- User: commit and start 39.
- 39 done: Schematic at 15 / 13 px text with zoom steps (Ctrl + wheel, Ctrl + / − / 0), in its own view (centred or scrolling), /view lines in editor pixels, Terms. Xvfb checked; host test with a display passes. 40 of 40.
- User: commit and do the crosscheck; looks great; on the Schematic let the blocks use the whole page (now limited to the old page size) → 39b.

## Crosscheck after Phases 38–39

- Committed 89b20f1. Code vs S6.21, S6.22: as built (noted there). Stale lines fixed: S6.5 (at most 6 rows, the rack scrolls), S6.7 (the view no longer fills the tab: text size and zoom), S6.16 (the window may be smaller than the rack). Plan header. 40 of 40. Open: CI after the push (38 mends macOS / Windows).
- Committed 89b20f1 (39) and eb5d74a (crosscheck). 39b done: the Schematic page as large as the view (top-left), blocks anywhere on it, the page grows 20 units beyond the furthest block, the legend at the page's bottom, stored layouts up to 1000 × 500; test_schematic; Xvfb (LFO 2 to the far right). 40 of 40.
- User: commit.
- User pushed 7a60ac9 (CI run 45839776544 watched: Ubuntu, Fedora, Windows green so far). User: multi-select on the Schematic to move a group? Proposed: rectangle on empty space, Ctrl / Shift-click, Ctrl + A, Escape; highlight box; group drag with preview; Shift + arrows; one undo step. User: all ok, both ways and undo; start the spec. S6.23 written; Plan 40; Tests planned. (Schematic moves were already undo steps.)
- CI run 45839776544 (7a60ac9) green on all four: macOS and Windows back to green (Phase 38 mended the host window check).
- User: commit the spec and start 40.
- Committed 0f23655 (spec). 40 done: Layout fitsGroup / moveGroup / blocksIn, selection and rectangle in SchematicView, group drag and Shift + arrows, Ctrl + A, Escape, highlight boxes, one undo step; tests; Xvfb checked. Found: Ctrl + A came as 'a' (lowercase) → fixed. 40 of 40.
- User: commit and do the crosscheck; looks good.

## Crosscheck after 39b and Phase 40

- Committed c3aba54. Code vs S6.22 (39b) and S6.23: as built (noted there). Stale line fixed: S6.7 Shift+arrows (the selection since S6.23). Plan header. 40 of 40. Open: CI after the push.
- User: what is next? Listed; user: let's make a release (1.7.0).

## Release 1.7.0

- Upgrade check (T-S9.6-upgrade-1.7): 1.6.0 download on Xvfb → sound, slopes, cables, notes, OUT moved, closed → the 1.7.0 build: /patch identical but for the new parameters at their defaults, slopes kept, layout kept with MIX 2 at the end of row 2 (scrolls), round trip ok.
- Prepared: CMakeLists 1.7.0, Changes "## 1.7.0 (2026-11-26)" with the upgrade note, Spec S6.10, Plan row 1.7.0, Tests; pictures retaken with the 1.7.0 title.
- User pushed and tagged v1.7.0, went home. Release run 45853403777 green (tag check, four builds, publish): 1.7.0 released with six files. Build runs 45853397596 (ab3569c) and 45850150536 (e1663ee) green on all four.
- After the release (CLAUDE.md rule): 4 releases (1.4.0–1.7.0) and 7 artifacts older than 14 days (2026-11-12) → cleanup proposed, waiting for the user.
- User: cleanup done (checked: releases 1.6.0 and 1.7.0 left, 0 artifacts older than 14 days, all tags kept); pushed 6407f5b (architecture only: no CI run).
- User: start the spec for L11. S6.24 / S5.27 proposed (to confirm): keys back to 28 px for about 400 px of room; Bend wheel (springs back), Mod wheel, Glide time + Always / Legato, Bend range 1 / 2 / 7 / 12; per-voice glide from the last note, exponential; keys.mod output; wheels are playing controls (not saved), glide / mode / range are parameters; /keys bend / mod; presets by ear. Plan 41.
- User: all ok, commit the spec and start 41.
- Committed b793781 (spec). 41 done: keys.mod output, per-voice glide (Always / Legato), bend range parameter, wheels in the processor (MIDI and the on-screen wheels / 9P), the strip (wheels, Glide horizontal with its box, Range), keys at 28 / 21 px, KEYS Mod socket, presets, Terms (Glide; Pitch bend, Mod wheel), AI guide; tests/test_glide.cpp, host /keys and count 88. Found: a vertical Glide fader had no room in the 90 px strip → horizontal. 41 of 41.
- User: the keyboard is half hidden on XFCE by the bottom panel. Cause: JUCE on Linux gives the whole monitor as user area. Fixed: the work area from _NET_WORKAREA (libX11 loaded at run time); Xvfb with a set work area 1920 × 1040 → window 1920 × 1040.
- User: resizing leaves grey lines and freezes for about 2 minutes; the resize goes on after mouse release. Measured: layout about 1 ms, a frame 10–20 ms; cause: JUCE's own title bar resizes per mouse move, queued. Fixed: on Linux the window manager's frame (resize, maximise to its work area, opens maximised, restore 5 % smaller); Options items now at the top of Menu. Xvfb + xfwm4: maximised under the frame, corner drag stops at release, Menu items and Reset work; no WM: fills the screen. S6.16 note, Changes, Tests T-S6.16-workarea / -frame. 41 of 41, u24 built. Open: user check on XFCE.
- User: commit and do the crosscheck; resize is now behaving perfect.
- Committed 8314f6a (Phase 41 + Linux window fixes; Tests: the user's XFCE check passed).

## Crosscheck after Phase 41 and the Linux window fixes

- Code vs S6.24 / S5.27 and S6.16: as built. Stale lines fixed: S6.11 (no Options button on Linux, its items in Menu), S6.2 (Linux title bar is the window manager's). Plan header. 41 of 41. Open: CI after the push; glide presets by ear (user).
- User: on Reset layout move MIXER ("mix1") after NOISE and FORMANT after VCF 2. Done (both 4 columns: widths unchanged); Xvfb checked.
- User (mid-turn): on the Schematic move the effects to the bottom. Done: effects row at y 70, canvas 84 high (test_schematic needed 4 units for the legend). S6.5, S6.7, Changes, Tests T-S6.5-default-2.
- User (mid-turn): could the effects be moved left and right, with the in / out chain re-arranged automatically? Spec proposal S7.14 written in the answer (to confirm).
- User: commit and start S7.14 as proposed.
- Committed fda7c5f (layout changes). S7.14 written, Plan 42. 42 done: effects.order (24 orders), normals follow the order (engine, GUI, /ports), processing in the order, Effects tab drag / arrows / menu, Schematic blocks swap (one undo step), /effects; tests/test_effectorder.cpp, host checks (89 parameters). Found: reverb first vs the cable chain is not bit-exact (the cable chain is one sample later and COMB's tuning / Mix smoothing follow the clock) → compared against the default order instead. Xvfb checked. 42 of 42.
- User: the glide and wheels are lost after switching to Effects / Matrix / Schematic; they should always be visible with the keyboard. Cause: showTab() hid every child not on its always-shown list. Fixed; Xvfb: the strip the same on all six tabs.
- User (mid-turn): Reset layout on the Effects tab should give the default effects order. Done (one step; rack layout and cables kept); Xvfb checked. S6.24, S7.14, Tests T-S6.24-tabs, Changes. 42 of 42.
- User: commit and do the crosscheck.
- Committed c788d85 (Phase 42 + fixes).

## Crosscheck after Phase 42

- Code vs S7.14, S6.24 (tabs fix), S6.5 / S6.7 (default rack, Schematic effects row): as built. Stale lines fixed: S7.6 (the normalled chain follows effects.order), S6.7 (Reset layout on the Effects tab). Plan header. 42 of 42. Open: push (60835b6 … this), CI; listening checks (glide presets, effect orders by ear).
- User pushed; CI run 45905730832 (eec3a66) watched. User: what is next? Listed (small: L1, L2, L20; medium: L27, L18, L21, L5, L9/L16; large: L3/L4, L12, 19/L14, L17, L6, 20, 22e; or release 1.8.0). User: was L2 not done by the socket menu faders? Yes: L2 closed as covered by S6.18 / S5.21.
- User: start the spec for L27. S7.15 proposed (FDN 8 lines, Householder, diffusers, early reflections, slow modulation; same four faders; level matched); choices asked: old sound (replace / Type Classic-Hall), Pre-delay fader, Mod fader. Plan 43.
- User: 1b, 2b, 3a ok; commit the spec and start 43. S7.15 confirmed (Type Classic / Hall, older patches and states Classic; Pre-delay 0–200 ms; fixed modulation).

## 2026-11-27 (continued in the same session)

- Committed bc7fe65 (spec 43). 43 done: src/dsp/Reverb.h (Hall FDN), reverb.type / reverb.predelay, Classic for older states (StateMigration 6) and patch files, presets Hall with Pre-delay, panel Type | Size | Damping | Width | Pre-delay | Mix; tests/test_reverb.cpp (with a Freeverb model for the ring), host checks for both types and the older-patch rule. Tuned by measurement: modulation 1 ms (ring 6.7 vs Freeverb 16.6), Damping a two-pole wet lowpass besides the decay (the decay alone left the start bright), the level by a fit of the measured raw level (flat within 0.2 dB). Found: the editor's faders[] / choices[] arrays overflowed (crash at start, ctest has no editor) → sized + hard check; the Type box was hidden on the Effects tab (choices not in the effect list) → fixed. Upgrade check with the 1.7.0 release: Classic kept. Pictures retaken (rack layout, REVERB). 43 of 43.
- User: continue (commit). Committed 7b3c10f (Phase 43).

## Crosscheck after Phase 43

- Code vs S7.15: as built. Found: the tail told to the host with Hall was the Hall's alone (Size 0: 1.3 s), even with DELAY (10 s) or COMB (2 s) on → now the longest tail of the effects in use; host test checks Hall 2.9 s, with DELAY 10 s. Stale Spec lines fixed: S5a.1 (keys.glide, keys.glidemode, keys.bendrange, effects.order, reverb.type, reverb.predelay; 91 parameters), S9.2 (91; 2172 host parameters, measured), S7.15 body (panel, older patches, Phase 35 lowpass, presets, Damping 1/3.3, tail). Tests T-S7.15-reverb. 43 of 43 pass.
- User: yes, commit; will push and wait for CI. Committed (crosscheck 43).
- User: the matrix section of the effects is not very helpful (typo "helpfull" noted); split it off in a separate effects matrix, free space in the synth matrix; tabs Synth, Matrix, Schematic first, then Effects. S6.25 proposed (Matrix synth only 34 × 19; effects 5 × 5; choices A where, B order).
- CI run 45963072216 started for 3379fa7.
- User: Aa, Ba ok; commit the spec and start 44. S6.25 confirmed (effects matrix on the Effects tab below the panels, following the effects order); Plan 44, L30; Tests T-S6.25-fxmatrix planned.
- Committed 77c1835 (spec 44). CI run 45963072216 (3379fa7): all four green.
- 44 done: Patch.h port lists (synth / effects matrix), MatrixView with row / column lists (setPorts, preferredSize), fxMatrix on the Effects tab (beside MAIN where it fits, else below; follows the effects order), tabs Synth | Matrix | Schematic | Effects | Notes | Assistant, /view "area … fxmatrix". test_fxmatrix (T-S6.25-fxmatrix). Xvfb :141: 5 × 5 beside MAIN, 9P cable shown, click makes a cable (one undo step), reversed order re-orders it; Matrix 34 × 19. Pictures retaken; Terms (Matrix, Where lines), Tutorial lessons 1, 6, 9; Spec S6.7, S6.8, S7.2, S7.4, S7.6, S6.25 as built; Plan 44 done; Tests; Changes. 44 of 44 pass.
- User: yes, commit and do the crosscheck.
- Committed 9e6c5ce (Phase 44).

## Crosscheck after Phase 44

- Code vs S6.25, S6.8, S7.6: as built (Patch.h port lists, MatrixView lists, fxMatrix with the Effects components, order follows, refresh / palette / undo as the Matrix). Stale: S7.12 /view area names (+ fxmatrix), PluginEditor.h comment, README (Matrix: every synth connection; Effects: with their own matrix). Plan header. No code change.
- User: units in the schematic whose output is not used effectively do nothing; stipple outline? (typos "unis", "schema", "who's", "effectivly" noted). S6.26 proposed (rule through levels, pass-through at Mix 0, where, lines); choices asked.
- User: 1b, 2b, 3b, 4a ok; commit the spec and start 45. S6.26 confirmed; Plan 45, L31; Tests T-S6.26-unused planned. Committed with the crosscheck fixes after 44.
- Committed 3dcd7bd (crosscheck 44 + spec 45). 45 done: Patch.h unitUse / unusedReason, processor useParams (faders, mod wheel, Mix 0 passing), editor updateUse each tick, dashed panel frames and title hover notes, Schematic dashed outlines, dotted lines, block hover note. test_unused (T-S6.26-unused). Xvfb :141 checked (default sound, live change, hover). Pictures retaken; Terms "No effect"; Tutorial lesson 1; Spec S6.7, S6.26 as built; Plan 45 done; Tests; Changes. 45 of 45 pass.
- User: yes, commit and do the crosscheck.
- Committed d25a007 (Phase 45).

## Crosscheck after Phase 45

- Code vs S6.26: the gains in useParams match Engine.h Voice::process (FM depth + mod wheel, PM index, MIXER / MIX 2 levels, VCF Env on any cutoff source, FORMANT Mix on vowel, Tremolo, Velocity; Res, CV, Pan, Rate, gates count when connected); effects and FORMANT at Mix 0 pass exactly (Spec S7.4, S5.16). Found: the Schematic legend had no sample of the new outlines → "no effect" and "Mix 0" boxes after the rings (Xvfb checked). Pictures retaken. Spec as built, Tests, Plan header. 45 of 45 pass.
- User: yes, commit; will push and wait for CI. Committed (crosscheck 45).
- User: what is next? Listed (listening checks, release 1.8.0; small L1, L20; medium L18, L21, L5, L9/L16; large L3/L4, L12, 19/L14, L17, L6, 20, 22e). User: start the spec for L1. S6.27 proposed (hover replaced normal, Matrix, silent loops); choices A, B.
- CI run 45982288039 (cbd9d73): ubuntu, windows, fedora green (macOS pending).
- User: Aa, Bb ok; commit the spec and start 46; asked whether VCFs generate sound at high resonance (typos "thier", "we we" noted). Answer: self-oscillation from ~91 % Res, but a voice from rest clears the filter to 0, so no input = silent; added as open choice C (keep / seed). Spec committed.
- Committed 008487f (spec 46). CI run 45982288039 (cbd9d73): all four green.
- 46 A and B done: Patch.h replacedNormals / replacedNote (socket and Schematic port hover), Matrix dashed ring + hover; soundingOutputs, silent units as no effect (followed through their audio inputs), "Silent: no sound reaches it (…)". test_hint (T-S6.27-hint, incl. the selfSounding hook for C). Xvfb :141: loop vcf1.in = vcf1.out — chain dashed, hovers, dashed ring. Pictures retaken (Lesson 6 shows the dashed ring); Tutorial Lesson 6, Terms No effect; Spec as built; Tests; Plan; Changes. 46 of 46 pass. Choice C still open.
- User: Cb ok; commit and do the crosscheck. Committed 46 A and B; C (seed) next, then the crosscheck.
- Committed 32fc9f3 (46 A and B). C done: Ladder start value 1e-3 from rest above k 4 (measured start times); engine test showed vcf1.out → vcf1.in at 100 % stays silent even at Res 100 % (the loop takes ~L off the feedback; rings up to ~40 % at Res 100 %); rule counts 4.4 × Res − loop > 4 or a Res cable; host check (no input: 50 % silent, 100 % rings, loop 100 % silent); test_hint Ladder checks. Terms Resonance, Tutorial Lesson 3, Spec S5.7 and S6.27 as built, Tests, Plan, Changes.

## Crosscheck after Phase 46

- Code vs S6.27 and S6.26: hover via Patch.h replacedNote in both describeInput paths (sockets on both tabs, Schematic ports), Matrix and effects matrix ring (same view), sounding through audio inputs with gains, selfSounding from the processor. The answer to the user's question refined by measurement (own loop lowers resonance) — in Spec as built. 46 of 46 pass.
- User: yes, commit; will push and wait for CI. Committed (46 C + crosscheck).
- Committed f65c698 (46 C + crosscheck). u24 refreshed. User: what is next? Listed. User: start the spec for L20. S6.28 proposed (measuring, memory, header, /load, AI, CI benchmark); choices A-D.
- User: yes all recommended; and drop L9 / L16 (typos "reccomened", "l9/l16" noted). S6.28 confirmed (A a, B a, C a, D a); Plan 47, L20; L9 and L16 dropped; Tests T-S6.28-load planned.
- Committed 6496448 (spec 47). 47 done: dsp/Load.h LoadMeter + loadText, processor block timer, voices, xruns from the standalone device, ProcessMemory.cpp, header label (red / Max: black), 9P /load, lmw-mcp get_load, Assistant tool line, guide; test_load (unit + benchmark 0.9 / 5.4 / 11.4 / 21.6 %), host /load, MCP 14 tools, Assistant 12. Xvfb: label, hover (found: a tooltip whose text changes every update never shows → frozen while hovered). Terms Load; Spec S10.10, S11 tools, S6.28 as built; Tests; Plan; Changes. 47 of 47 pass. f65c698 not pushed yet (latest CI cbd9d73 green).
- User: yes, commit and do the crosscheck.
- Committed 8d24cb3 (Phase 47).

## Crosscheck after Phase 47

- Code vs S6.28, S10.10, S11: as built. Found: a DAW's offline render (bounce) slower than real time would count overruns and turn the meter red → LoadMeter::block(…, realtime) with !isNonRealtime(); unit check added. std::atomic<double> static_assert lock-free. Spec as built, Tests, Plan header.
- User: yes, commit; will push and wait for CI. Committed (crosscheck 47).
- User: what is next? Listed (push, listening, release 1.8.0; medium L18, L21, L5; large ...). User: lets go for v1.8.0. CMakeLists 1.8.0; Changes "## 1.8.0 (2026-11-27)" with the upgrade note (1.7.0 patches and states: reverb Classic, effects order as before; VCF above ~91 % Reso without input now sings); pictures retaken (title 1.8.0). 47 of 47 pass. CI run 46000596436 (ee5de8a) started (user pushed).
- Committed 9d1a17d (release 1.8.0 prepared). User: done: glide, effects orders, the Hall presets, and the load meter while playing. Recorded as listening pass (Tests T-S6.24-playing, T-S7.14-order, T-S7.15-reverb, T-S6.28-load; Plan 41, 42, 43, 47); the singing VCF (S6.27 C) by ear still open.
- User: yes, commit; will tag after CI. CI run 46001152016 (9d1a17d) started.
- Committed bf32fdf. CI ee5de8a and 9d1a17d: ubuntu and fedora green, Windows failed: T-S6.27-hint — MSVC turned "→" in Patch.h into "?" (the hover text on Windows too). Fixed with UTF-8 byte escapes; the same old bug in Schematic.h (internal-link notes →, ×) fixed; tests use the bytes too. Load benchmark: RUN_SERIAL and best of 3 (failed once locally under a parallel ctest + container build). 47 of 47 pass. Tag v1.8.0 to wait for the fix's CI. u24 rebuilt at 1.8.0 (before this fix).
- CI 5df7d85 (run 46004132428): all four green. User tagged v1.8.0; release run 46007508534 started (tag check ok). User: what is next? Listed. User: start the spec for L18. S6.29 proposed (base, presets, scope, unit tints, contrast, edit, config, tests); choices A-E.
- User: all recommended; commit the spec and start 48. S6.29 confirmed; Plan 48, L18; Tests T-S6.29-dark planned.
- Committed 11926c0 (spec 48). Release run 46007508534: LMW-Synth 1.8.0 published (6 files). Cleanup check: 3 releases but no release and no artifact older than 14 days (331 artifacts) — nothing to clean, no warning.
- 48 done: Theme.h foreground / background / keyboard in the palette, Dark and DarkHC presets, System, current(); ThemeInk.h (kFg / kBg / kQuiet read the current theme, smallFont, retintDesktop); LookAndFeel applyTheme; Config base / color_text / color_page; Theme menu; Follow system poll; ThemeDialog contrasts against the theme; placeholders refreshed (found on Xvfb); Max detection by page colour; scan test. test_contrast extended. Xvfb: all tabs in Dark, live switches, Edit colours. Terms Theme / Contrast; Spec as built; Tests; Plan; Changes. 48 of 48 pass.
- User: yes, commit and do the crosscheck.
- Committed 845dfd2 (Phase 48).

## Crosscheck after Phase 48

- Code vs S6.29, S6.13, S6.15, S6.28: as built. Found: keyboard details (pressed white key note name, hover rings, pressed black key edge) drawn with black / white or the page colour, not the keyboard's own colours that Palette::pairs checks — in Dark the black-key hover ring was invisible → keyText / whiteKey / blackKey. Spec S6.13 note, as built; Plan header.
- User: yes, commit; will push and wait for CI. Committed (crosscheck 48).
- User: what is next? Listed. User: start the spec for L21. S5.28 proposed (Shape per VCO, per wave, band-limited, panel, presets); choices A-D.
- User: all recommended; commit the spec and start 49; should we also allow that on the LFO? Answer: yes, it makes sense, but a 4th LFO column would not fit 1920 with LFO 1 in row 3; open choice E (fader under the Wave box / own column / VCOs only). S5.28 confirmed (A a, B a, C a, D a); Plan 49, L21; Tests T-S5.28-shape planned.
- User (mid-turn): short fader is ok — choice E a (LFO Shape under the Wave box).
- 49 done: Oscillator / Lfo shapes (measured: alias, mean; widths at least 2 samples, the sine's mean removed — both found by measuring), Patch inputs vco1-3.shape / lfo1-2.shape, engine, 5 parameters (96), VCO Shape column, LFO Wave over a short Shape fader (scale texts hidden under 60 px — found on Xvfb), LFO 1 to row 3, Schematic symbols taller (found on Xvfb), presets PWM (Shape 50 %) and PWM strings; test_shape; counts in 7 tests; Terms Shape; Tutorial Lesson 2 step 6; Spec as built, S5a.1, S9.2, S6.8; Tests; Plan; Changes. 49 of 49 pass.
- User (mid-turn): now that we have several more enhancements, should we go over the presets? … if you want to group types of instruments (woodwind/keyboard/...) feel free. To be specified after 49.
- User pushed (c6c2b68 and before); CI watched.
- User: yes, commit and do the crosscheck.
- Committed dde9c96 (Phase 49).

## Crosscheck after Phase 49

- Code vs S5.28: as built; old patches and states load with Shape 0 (the default) — the same sound, no migration needed; the S6.26 rule counts the Shape inputs (gain 1 when cabled); vcoUsed counts a VCO read by a Shape input. Stale and fixed: Spec S6.5 (default layout), S6.6 (VCO and LFO socket lists), docs/ai/SoundDesign.md (vcoN.shape, lfoN.shape, a PWM recipe), the LFO Shape hover (per-voice sources: newest note). Plan header.
- User: yes, commit and start the preset review spec. Committed (crosscheck 49).
- Committed 710d2da. S7.16 preset review proposed (groups, improvements, 6 new Synth presets, loudness); choices A-D.
- User: all recommended; commit the spec and start 50; also add each type of world instrument to its class (typo "windwood" noted). Read as: World stays, each world instrument also in its family. S7.16 confirmed; Plan 50, L32; Tests planned.
- User (mid-turn): adding a second synth only works with two key ranks or a split keyboard? Answer: also layering (same keys) or MIDI channels; a split keyboard to be added to L17 (split point; layer / split / channel per synth) — Plan update pending.
- 50 in progress (uncommitted): families + menu + MCP list, improvements, 6 Synth presets, tests (instrument families, new presets), host loudness check 4h-2 (loudest 50 ms of a held C4). User: loudness a is ok (all ~9 dB below the old String section; OUT Volume per instrument). 39 of 40 within +-2 dB; Duduk opened further (cutoff 4000, formant 0.3) — not yet re-measured.
- User: stop here, continue at home. Open: re-measure Duduk, docs (Spec as built, Tests, Plan 50 + L17 split, Changes, Terms?), pictures, host test on display, u24, commit.
- (Continued at home on the same VM.) User: ok continue. Duduk: opening the filter alone gave −5.6 dB; the cause was its 12.5 % pulse — now a square at Shape 40 % (a 32 % pulse), −1.8 dB: 40 of 40 within ±2 dB. Tutorial menu paths and Terms updated; Spec S7.16 as built; Tests; Plan 50 done, L17 with the split keyboard (layer / split / channel); Changes. 49 of 49 pass.
- Host test on display passes; pictures retaken; u24 build 49 of 49. User: yes, commit and do the crosscheck. Committed Phase 50; crosscheck next.
- Crosscheck after Phase 50: code matched the Spec (families, menu, list_presets, OUT Volume). Stale docs fixed: Spec S7.7 count / menu, S7.13 menu note, S11 list_presets; MCP list_presets text; Terms Instrument preset; AI guide (families, Synth presets, velocity cable, levels). 49 of 49 pass.
- User: yes, commit; I will push and wait for CI. Committed the crosscheck fixes.
- User: lets go for v1.9.0; listen test all ok. Version 1.9.0, Changes 1.9.0 with the upgrade note, Spec S6.10 version list (1.8.0 and 1.9.0 added), Tests / Plan listening pass.
- User: can we add one alphabetical list of All presets under All? (typos noted: alpabetical, undr). Proposed A a (after World), B a (built-ins only, A–Z ignoring case), C a (in 1.9.0); user: all recommended. Built: Presets › All; test names unique ignoring case; Spec S7.7 / S7.16, Terms, Changes, Tests.
- Presets › All checked on Xvfb (40 A–Z, fits 1080 px, Kalimba applies); pictures retaken (1.9.0 in the header); 49 of 49 pass.
- User: yes, commit; I will push and tag after CI. Committed Release 1.9.0.
- v1.9.0 tagged by the user; cleanup (user): releases v1.6.0, v1.7.0 deleted (tags kept), 32 artifacts older than 14 days deleted.
- v1.9.0 release published (6 assets); 2 releases left (v1.9.0, v1.8.0).
- User: move the effects schema to the effects page? an amplify fader on the final output stage, or redundant? a Resonance input on FORMANT? a base level on the LFO (positive for VCA modulation, unless built in with Tremolo)? (typo noted: "inless" → unless). Answer: 1 yes, an EFFECTS block in the Schematic + a Panels / Matrix / Schematic switch on the Effects tab; 2 mostly redundant — OUT Volume to +12 dB and a MAIN limiter light instead; 3 yes, cheap; 4 Tremolo is already one-sided, but VCA CV / Shape / Cutoff / Vowel lose half the wave — a Polarity ± / + / − recommended. Order 3 + 4, 2, 1.
- User: yes, add to plan and start the spec for 3 + 4. Plan L33–L36, Phase 51; Spec S5.29 (FORMANT Res input), S5.30 (LFO Polarity) proposed with choices A–E; Tests planned.
- User: all recommended; commit the spec and start 51. S5.29 / S5.30 confirmed (A a, B a, C a, D a, E a); committed; Phase 51 started.
- User (mid-turn): tested the whole shebang on Windows (1.9.0); looks and sounds great.
- 51 built: formant.res input (engine, Matrix after formant.vowel, socket under Reso, Schematic port), LFO Polarity (Lfo.h polarize, params Bipolar / Positive / Negative, box ±/+/−). Xvfb: under the Wave box the Shape fader was left only its handle → the Polarity box moved under the Delay fader, above Out (as OUT's Spread mode); choice D changed, to confirm with the user. test_polarity; counts (45 inputs, 26 CV, Matrix 40 × 19, 98 parameters, 2179 VST3); Terms Polarity (89); tutorial Lesson 5 step; AI guide; MCP text; Spec, Tests, Plan, Changes; host test with a display passes; pictures retaken; 50 of 50.
- User (mid-turn): note the default synth layout: Rack1 KEYS, LFO1, VCO1-3; Rack2 Mix1, VCF1, VCF2, FORMANT; Rack3 Mix2, ENV2, LFO2, ENV1, VCA, OUT (typo noted: "node" → note). Asked where NOISE goes.
- User: add a theme option: a gradient from a unit's coloured title bar down to white (light) / the dark end colour (dark) (typo noted: "more someting" → mode something). Proposed spec L37 / Phase 52 with choices A–E (not yet answered).
- User: Rack1: KEYS, LFO1, NOISE, VCO1, VCO2, VCO3. Built as 51b: the default order by title in the editor (replaces the moveBefore steps); Xvfb fresh HOME: rows as asked, row 1 ends at 1902 px; pictures retaken; Spec S6.5, Tests T-S6.5-default-3, Plan 51b, Changes. 50 of 50.
- User: yes, commit; all recommended for 52; CI covers u24; a horizontal fader example snapshot in the tutorial as well (read as: a picture of the panel gradient — asked to confirm). Committed 51 + 51b.
- (2026-11-28, log continued here.) 52 built: Menu › Theme › Panel gradient (config panel_gradient, /view gradient), Palette::gradientTop keeps text and quiet text at the contrast setting (82 of 168 tops moved), synth and effect panels fade to the page above the sockets; unit test in test_contrast; Xvfb light and dark, Effects tab, menu tick / toggle / saved; tutorial Lesson 1 picture lesson1-gradient.png (pictures.sh). "Horizontal fader example snapshot" read as a picture of the gradient — to confirm with the user.
- User: yes, commit and do the crosscheck. Committed Phase 52; crosscheck next (51, 51b, 52).
- Crosscheck after 51, 51b, 52: code matched the Spec. Stale docs fixed: Spec S5.29 FORMANT socket order, S5.30 choice D as built (under Delay), S5.3 normals (formant.res none), S6.8 Matrix rows 40, S9.2 parameters 98, notes in S5.28 / S5.26 on the old default layout; Terms Polarity (under Delay) and Theme (Panel gradient); Plan header.
- User: yes, commit; I will push and wait for CI. Committed the crosscheck fixes.
- User: is the local build-u24 rebuilt? No (from Phase 50); rebuilt in the container: rc 0, 50 of 50.
- User: ok continue. CI for aef6313 green (4 of 4). Next in the agreed order: L34 → Phase 53, Spec S5.31 (OUT Volume to +12 dB, limiter lamps) proposed with choices A–E; Plan, Tests planned.
- User: all recommended; commit the spec and start 53. S5.31 confirmed (A a … E a); committed; Phase 53 started.
- 53 built: out.gain 0–4 (skew: 0 dB at 75 %), dB scale kind, '+6.0 dB' text; LimitMeter / reductionDb, Engine::takeOutPeak, MAIN peak; /load limit_out / limit_main; LimitLamp in OUT (title bar) and MAIN (under the title); Terms Limiter (90), Volume; tutorial Lesson 1; AI guide; MCP text; test_outgain + test updates; 51 of 51 incl. host with a display; Xvfb lamps off / lit, hover.
- Pictures: lesson5-vca.png was a sliver since 51b (VCA right of the 1496 px pictures window) and row-3 crops had the scroll bar over the socket labels → pictures.sh window 1920 × 1000 and shot() scrolls to a panel right of the view; all pictures retaken; Spec S7.12 note.
- User: yes, commit and do the crosscheck. Committed Phase 53 with the pictures fix; crosscheck next.
- Crosscheck after 53: code matched the Spec. Stale lines fixed: S5a.1 out.gain 0–4, S10.10 /load (limit lines) and /info scale words (db); Plan header.
- User: yes, commit; I will push and wait for CI. Committed the crosscheck fixes.
