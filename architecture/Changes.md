# Changes

Changes between releases.

## Unreleased
- Project started: architecture docs (Spec, Plan, Tests, conversation log).
- Decisions: C++20, JUCE 8, AGPLv3, semi-modular design, standalone + VST3 targets.
- Added LICENSE (GNU AGPLv3).
- Decided GUI/engine split over self-implemented 9P2000; external GUI in Kukel.
- Phase 0: CMake + JUCE 8.0.4 skeleton (Standalone + VST3, outputs silence), WCAG contrast module + tests, GitHub Actions CI for Linux/macOS/Windows.
- Phase 1: single-voice engine (polyBLEP VCO: sine/saw/square/triangle; linear ADSR; smoothed volume), parameters saved with host/standalone state, GUI with knobs + on-screen/QWERTY keyboard, black-on-white theme with tested colour pairs.
- Phase 2: polyphony 1–32 (voice.count), oldest-voice stealing preferring released voices, same-note retrigger, click-free stealing.
- Added architecture/Handover.md.
- CI: `actions/checkout` v4 → v7 (Node.js 24; removes Node 20 deprecation warning).
- CI: `actions/checkout` pinned to commit SHA `3d3c42e5…` (v7.0.1); all actions are now pinned by hash (Spec S2.3).
- JUCE 8.0.4 → 8.0.17-security (security fixes: WAV/AIFF, ZipFile, XML, GIF/PNG, OSC, JSON, ValueTree parsing); pinned to commit `dabd5ad1…` archive + SHA-256 (Spec S2.3).
- JUCE 8.0.17-security → 9.0.5, pinned to commit `d60b7a78…` archive + SHA-256. CI Linux deps: added libxi-dev, libegl-dev (JUCE 9 requirements). Spec S2.2 now JUCE 9. No source changes needed.
- Linux targets: Ubuntu 24.04+ and Fedora latest (45). CI: ubuntu-latest → ubuntu-24.04 (binaries run on 24.04 and newer); new Fedora 45 job in container pinned by digest. CMake: policy CMP0135 NEW (fixes FetchContent timestamp warning).
- GUI: rotary knobs replaced by vertical faders (black/white only: thin track, thick value bar, black cap with white line), grouped VCO | ENV | OUT; window 628×450.
- GUI fix: slider value boxes were white text on white (JUCE default colours, created before our look-and-feel was set) — now black on white.
- Parameter values shown with units (ms/s, %, dB) in the GUI and in plugin hosts; typed values accept units. New test T-S6.1-text.
- `scripts/build-ubuntu24.sh`: container build for Ubuntu 24.04+ (image pinned by digest, cached build volume, podman or docker) → `build-u24/`.
- Fix: computer-keyboard notes stopped working after clicking a fader or the Wave box (focus moved away from the on-screen keyboard). Letter keys now play from any control, except while typing into a value box.
- Fix (Spec S10.2): the audio thread no longer locks or allocates — on-screen/QWERTY notes reach the engine through a lock-free SPSC queue; external MIDI notes are shown on the keyboard via a second queue; MIDI is decoded from raw bytes.
- New: sustain pedal (CC64) and pitch bend (±2 semitones). Tests: T-S3.2-midi, T-S3.3, T-S3.4, T-S10.2-spsc (4 test suites).
- Removed obsolete `JUCE_DISPLAY_SPLASH_SCREEN=0` (ignored by JUCE 9, caused a build warning). Spec S8.1: licence is AGPL-3.0-or-later.
- Soft limiter on the output (Spec S4.6): transparent below -6 dBFS, tanh knee, ceiling -0.04 dBFS. Spec: S4.2 stealing order and S5.2 default patch confirmed; S5a.3 parameter-change behaviour.
- Renamed product "LMW Synth" → `LMW-Synth` (app file, VST3 bundle, window title, host name; plugin ID unchanged). Build script clears stale outputs before building.
- GUI: Voices fader moved to the far left in its own POLY group; window 614×450.
- Phase 3: VCF (24 dB ladder, resonance, self-oscillation), shared LFO (4 shapes) and noise, mixer, second envelope (→ VCF cutoff), mod wheel vibrato, normalled patch with cable override (engine + lock-free patch queue), sustain-level glide. 19 parameters (env.* renamed env1.*), new value units (Hz/kHz, oct, st). GUI in two rows. New test suite T-S5-modules (5 suites).
- CI: JUCE archive cached with `actions/cache` v6.1.0 (pinned by SHA) and fetched with retries by `cmake/FetchJuceArchive.cmake`; JUCE pin moved to `cmake/JuceVersion.cmake` (one place). Prompted by a GitHub 504 on the archive download that failed macOS/Windows.
- VCO FM now works without a MIDI controller: the LFO always reaches vco.fm; depth = FM fader (default 0) + mod wheel × 1 st. New VCA panel with an Initial gain (vca.initial, adds to ENV 1; > 0 = drone). Hover texts (after 2 s) on faders show input source and effect. Compact view toggle (remembered). Choice boxes drawn with a narrow arrow so names fit.
- Plan: Phase 4 step 1 is the rack (movable module panels snapping into rows, saved layout, Reset layout, keyboard/menu moves); cables follow as step 2. New Spec S6.5, planned tests T-S6.5a–c.
- Phase 4 step 1, rack (Spec S6.5): module panels can be dragged by their title bar into other row positions or new rows (max 4 rows, row width capped so the window width stays), with a drop marker; keyboard moves (arrow keys on a focused title) and a black-on-white move menu; layout saved (uiLayout) with Reset layout button. Pure layout model `src/ui/RackLayout.h` with unit test T-S6.5-layout (6 suites). Window height now includes the gap that was missing below the second row (1046×668 / 774×502).
- Phase 4 step 2, cables (Spec S6.6): socket row on every panel (inputs white, outputs black), drag to patch / move / remove cables, cables drawn above the panels and following rack moves and compact view, keyboard and menu patching, Delete removes; cables saved in the state as text (`src/dsp/PatchText.h`, unit test T-S5.4-text, 7 suites). Menus no longer show greyed (low-contrast) items. Rows are 36/30 px taller for the sockets (window 1046×740 / compact 774×562).
- Hover texts and cable menu headers now read in signal-flow order, source left: "lfo.out → vco.fm (normalled)" (was "Input vco.fm ← lfo.out"); Spec S5.3 port list written the same way (user request).
- VCO panel: FM fader moved to the left, above the FM input socket; Wave second (user request).
- Plan: Phase 4 step 2b, KEYS module (pitch, gate and velocity outputs; vco.pitch, env1.gate, env2.gate inputs), Spec S5.8, planned tests T-S5.8a–e.
- Phase 4 step 2b, KEYS module (Spec S5.8): new outputs keys.pitch/keys.gate/keys.vel and inputs vco.pitch/env1.gate/env2.gate (normalled from the keys); envelopes gated by other signals follow rising/falling edges; ENV1 gated by a cable counts for the drone rule. Port enums reordered (keys first; saved patches use names). POLY panel becomes KEYS, 2 columns, first in row 2; sockets placed under the fader column they belong to; socket labels shrink to fit. Default sound bit-identical. New test suite T-S5.8-keys (8 suites). Window 956×740 / compact 708×562.
- Plan: Phase 4b (VCF modes, VCO 1–3 with a 4-input mixer, VCF 2 in series and open by default) before contrast and theming; Spec S5.9–S5.11; planned tests T-S5.9–T-S5.11.
- Phase 4b a, VCF modes (Spec S5.9): Slope 24/12/6 dB and Type Low/High/Band/Notch from the ladder's stage outputs (Xpander-style mix), 20 ms crossfade on change, 24 dB lowpass keeps the original path (bit-identical default). Parameters vcf.slope, vcf.type; VCF panel gets a fourth column with two stacked choice boxes (hover texts). New test suite T-S5.9-vcfmodes (9 suites). Window 1036×740 / compact 768×562.
- Phase 4b b, VCO 1–3 (Spec S5.10): three VCOs per voice (FM, Tune, Wave, Oct each; sockets FM, Pitch, Out), 4-input mixer (VCO 1–3, Noise), VCOs computed only when used; names vco.* → vco1.*, mix.vco → mix.vco1, noise input mix.in2 → mix.in4, with translation of older saved states (`src/app/StateMigration.h`, stateVersion 2). Default layout three rows; row height reduced (window 1026×884 / compact 762×678). Default sound bit-identical. New test suite T-S5.10-vcos (10 suites).
- Phase 4b c, VCF 2 (Spec S5.11): second VCF in series (vcf1 → vcf2 → vca) with its own cutoff/reso/env/slope/type; exact bypass while open (24 dB LP, no resonance, cutoff at max), 20 ms crossfade in/out; vcf.* renamed vcf1.*, state version 3 with step-by-step translation of older states. Layout: MIXER | VCF 1 | VCF 2 in row 2, VCA at the end of row 3. Default sound bit-identical. New test suite T-S5.11-vcf2 (11 suites).
- Phase 4 step 3, contrast (Spec S6.3): Contrast box in the header (AA / AAA / Max / Custom with a ratio dialog), config.ini (contrast, contrast_ratio, compact) read on open and written on change; palette raises the pressed-key colours to the chosen ratio (inverted at Max), custom keyboard drawing (note name contrast, hover outline, solid octave arrows — JUCE's were ~2.8:1). Custom ratio floor 4.5. TextEditor/AlertWindow colours themed. Plan: schematic view idea (S6.7) recorded.
- Phase 4 step 4, theming (Spec S6.2): custom standalone app installs the LMW look-and-feel first, so the title bar (white, black text, line), window buttons (black glyphs), Options menu (solid separators) and audio/MIDI settings dialog match the main panel.
- Fix: Fedora 45 VST3 link failure (`ld.bfd: failed to set dynamic section sizes: bad value`, CI run 42628409040): lmw_ui_core now position-independent; it is linked into the VST3 since the contrast palette calls Contrast.cpp. Found with lld ("relocation R_X86_64_32S ... recompile with -fPIC"); verified in a local Fedora 45 container.
- VST3 editor checked on Linux in JUCE's AudioPluginHost: loads, draws, faders work, Compact resizes the host window.
- Phase 5, patch files (Spec S7): Load.../Save... (.lmwpatch plain text: all parameters exact, cables, notes; missing parameters → defaults; older names translated; bad files refused with a message), Panel/Notes tabs with free-text notes saved in the patch and the state, patch name in the header. LMW look installed as JUCE default while an editor is open (themed dialogs in DAWs); file rows and disabled buttons redrawn without greys. New test suite T-S7.1-patchfile (12 suites).
- Phase 6a, 9P2000 server core (Spec S10.3, S10.10–S10.12): src/ninep/Message.h (encoding) and Server.h (synthetic file tree of read/write callbacks, per-connection Session). Fuzzing found an out-of-bounds read on directory reads at a non-entry offset → offsets now validated. New test suite T-S10.1a-ninep (13 suites).
- Phase 6b, 9P transport and wiring (Spec S10.4, S10.5, S10.10–S10.14): Unix socket per instance (owner-only, stale sockets removed), optional TCP (config ninep_tcp, localhost by default), connection threads with polled I/O; NinePService maps parameters, cables, patch, notes, ports and source, applying changes on the message thread; 9P address as hover text of the title; value texts shared with patch files (now 7 significant digits); tool lmw9p. New test suite T-S10.4-socket (14 suites).
- Crosscheck 2026-11-09 (sub-agent, verified): choice values over 9P/patch files now matched by name, name without " dB"/"+", unique prefix, then index ("24", "1" → +1, "low"); 9P waits at most 5 s for the message thread ("busy"); stale Spec/Tests/Plan wording and code comments fixed (old vco./vcf. names, POLY, row numbers, /ports direction, error strings, float precision, config keys, duplicate test ids); S2.3 documents the runner/dnf pinning exceptions.
- Fix (CI run 42653555412, Windows): T-S10.4-socket ended with 0xc0000409 (uncaught exception); most likely std::filesystem on an AF_UNIX socket file (a reparse point on Windows). Socket files are now removed with C remove(), the test checks existence by listing the directory, and reports any exception text.
- CI runners fixed to version labels macos-26 and windows-2025 (were -latest; Spec S2.3). Cables from a state restored off the message thread are applied on the message thread (single producer of the cable queue; was a known limit).
- Phase 6c: conformance with plan9port's 9p client (built from source) and a load test (lmw9p bench: 1000 writes/s, audio thread syscalls unchanged, no xruns). lmw9p gained `bench`.
- Phase 8a: host test T-S9.2-host (JUCE VST3 hosting of the built plugin: scan, play, state save/restore into a second instance, two instances with distinct 9P sockets, editor). It found an engine bug: prepareToPlay did not reset the output gain smoothing (and noise state), so a re-prepared instance sounded different from a fresh one for the first 20 ms → reset (default sound still bit-identical).

- 2026-11-09: Schematic view (S6.7) scheduled as Phase 9 after Phase 8 (9a view-only, 9b editable).

- 2026-11-10: CI uploads a downloadable build per platform (S9.3): standalone, VST3, lmw9p, LICENSE, BUILD.txt.

- 2026-11-10: Phase 8b: Steinberg validator and pluginval (strictness 10) pass; scripts/validate-vst3.sh; fix: the single program is named "Default".
- 2026-11-10: CI runs the Steinberg validator and pluginval on all four platforms (cached builds).
- 2026-11-12: Phase 9a: LFO 2 + VCA tremolo (vca.mod, vca.tremolo), lfo.* -> lfo1.* (state version 4). Phase 9b: tabs Synth | Effects | Notes; reverb on the Effects tab.
- 2026-11-12: Phase 9c: VCF Key fader (key tracking) and Res input (no normalled source); Phase 9 done.
- 2026-11-12: Phase 10a/b: pin matrix model (up to 4 cables per input with levels, summed); several cables per input on the Synth tab; 9P /cables and patch files with levels.
- 2026-11-12: Phase 10c: Matrix tab (pin matrix with levels); Phase 10 done.
- 2026-11-12: Phase 11: NOISE panel (White/Pink/Brown, sample & hold random with Rate and Smooth, output noise.rand); LFO 2 next to the VCA.
- 2026-11-12: Crosscheck Spec/Plan/Tests/code after Phase 11: docs brought up to date (51 parameters, 16 matrix columns, tabs, patch-file pins, compact 948×678); older-version cable texts with pins now fully translated on load.
- 2026-11-12: Unit colours (S6.9): title bars and Matrix labels tinted per unit type (VCO red, NOISE orange, VCF yellow, VCA green, ENV blue, LFO purple), black text ≥ 11.8:1, white at Max.
- 2026-11-13: Phase 12a: Schematic tab (view only) — triangles for VCO/MIX/VCF/VCA/LFO, ADSR boxes, KEYS keyboard, NOISE trace, OUT circle with arrow, in unit colours; normalled lines dashed, cables solid with levels; hover texts.
- 2026-11-13: Schematic: internal links (dotted arrows into the module) for key tracking (while Key > 0) and velocity → VCA; mod wheel named in the VCO hover text.
- 2026-11-13: Phase 12b: editing in the Schematic tab like the Synth tab (drag to add/move/remove cables, port cursor, cable menu, Delete); drag rules shared in ui/CableEdit.h. Phase 12 done.
