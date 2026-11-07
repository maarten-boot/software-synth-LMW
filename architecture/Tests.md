# Tests — software-synth-LMW

| ID | Spec | Test | Status |
|---|---|---|---|
| T-S2.1 | S2.1 | Project builds and starts on Linux, macOS, Windows | Build + ctest pass on all (CI run 42406322210); starts on Linux (user on Ubuntu 24.04; Xvfb screenshot); start on macOS/Windows not verified |
| T-S2.1b | S2.1 | CI builds + tests on Ubuntu 24.04 and Fedora 45 (container, pinned by digest); Ubuntu-24.04-built standalone starts on Ubuntu 24.04 | pass: CI run 42406322210 (ubuntu-24.04, fedora-45, macos, windows; 3 test suites each); user ran Ubuntu 24.04 build (layout OK) |
| T-S2.1c | S2.1 | `scripts/build-ubuntu24.sh` builds + tests in an Ubuntu 24.04 container (image pinned by digest) and produces binaries needing glibc <= 2.39 | pass (podman rootless, 2026-11-07: GLIBC_2.38, 3/3 tests) |
| T-S2.3 | S2.3 | Every `uses:` in `.github/workflows/*.yml` references a 40-hex commit SHA with a `# vX.Y.Z` comment, and FetchContent dependencies are pinned by commit + URL_HASH; each is the latest workable release (or the exception is documented) | pass (checkout@3d3c42e = v7.0.1; JUCE d60b7a7 = 9.0.5, latest — 2026-11-07) |
| T-S1.2a | S1.2 | With no cables, default patch produces sound on a MIDI note | planned |
| T-S1.2b | S1.2 | A cable into an input replaces (not adds to) its normalled source | planned |
| T-S3.2 | S3.2 | Computer keyboard keys trigger notes in standalone, also after clicking a fader; typing in a value box plays no notes | pass (Xvfb + xdotool 2026-11-07: 'a' → C4 at start and after fader click; typing "250 ms" sets Attack, no note). User confirmed on Ubuntu 24.04 (2026-11-07), also after clicking/changing a fader |
| T-S3.2-midi | S3.2 | Raw MIDI decoding: note on/off (vel 0 = off), CC64, CC120/123, pitch bend, SysEx/truncated ignored | pass (`test_engine`) |
| T-S3.3 | S3.3 | Sustain pedal holds released notes; pedal up releases only notes whose key is up; same-note retrigger; all-notes-off lifts pedal | pass (`test_engine`) |
| T-S3.4 | S3.4 | Pitch bend +2 / -12 semitones on a sounding A4 gives 494 / 220 Hz | pass (`test_engine`) |
| T-S3.5 | S3.5 | Mod wheel sets vibrato depth | planned (Phase 3) |
| T-S5.1-osc | S5.1 | Each waveform has correct pitch (440 Hz ±1) and bounded output | pass (`test_engine`) |
| T-S5.1-adsr | S5.1 | ADSR attack/decay/release durations exact to ±2 samples; zero times safe | pass (`test_engine`) |
| T-S5.1-engine | S5.1 | Silence before note, sound during, silence after release; only matching note-off releases | pass (`test_engine`) |
| T-S6.2b | S6.2 | All theme colour pairs (src/ui/Theme.h) >= 4.5:1 | pass (`test_contrast`) |
| T-S6.1a | S6.1 | Controls are vertical faders grouped VCO / ENV / OUT; value box under each fader shows the value with units | pass (screenshot on Xvfb 2026-11-07: 10 ms, 200 ms, 70 %, 312 ms, -1.9 dB, 12) |
| T-S6.1b | S6.1 | Double-click on a fader resets it to the parameter default | pass (user, Ubuntu 24.04, 2026-11-07) |
| T-S6.1-text | S6.1 | Value text/parse: time ms/s, sustain %, volume dB, round trip | pass (`test_paramtext`) |
| T-S6.2c | S6.2 | Slider value boxes use theme colours (black text/outline on white), not JUCE default dark scheme | pass (screenshot; fixed by re-sending look-and-feel) |
| T-S6.2d | S6.2 | Standalone title bar, Options menu and audio/MIDI settings dialog use the main theme (white bg, black text, >= 4.5:1) | open — still JUCE dark default (user confirmed: must match main panel later) |
| T-S4.0 | S4, S3.1 | Sound is heard and several notes sound at once (manual) | pass (user, Ubuntu 24.04, 2026-11-07) |
| T-S4.1 | S4.1 | Setting polyphony to N allows exactly N simultaneous notes (N=1,8,32); range clamps to 1..32 | pass (`test_engine`) |
| T-S4.2 | S4.2 | Playing N+1 notes steals the oldest voice; released voices stolen first | pass (`test_engine`) |
| T-S4.3 | S4.3 | Same note twice uses one voice | pass (`test_engine`) |
| T-S4.4 | S4.4 | Steal causes no sample jump > 0.02 | pass (`test_engine`) |
| T-S4.5 | S4.5 | Lowering voice count 8→2 leaves 2 active after release | pass (`test_engine`) |
| T-S4.6 | S4.6 | Limiter transparent below -6 dBFS, continuous, monotonic, symmetric, <= ceiling; 32 square voices at full level stay below full scale | pass (`test_engine`) |
| T-S5a.3 | S5a.3 | Sustain-level change on a held note glides without a jump | planned (Phase 3) |
| T-S5a.1 | S5a.1 | Parameter IDs, ranges and defaults match Spec (vco.wave, env.*, out.gain, voice.count) | planned (no test yet) |
| T-S5a.2 | S5a.2 | A note-on at sample k of a block starts sound at sample k (not at block start) | planned (implemented in processBlock, no test yet) |
| T-S6.2 | S6.2 | Every UI colour pair computes >= 4.5:1 contrast | unit test `test_contrast` passes; UI-wide check pending |
| T-S6.3 | S6.3 | Each preset/custom ratio from config and UI is enforced | enforcer unit-tested; config/UI pending |
| T-S7.1 | S7.1 | Save then load a patch restores all parameters and cables exactly | planned |
| T-S8.1 | S8.1 | Every file in src/, tests/, scripts/ has `SPDX-License-Identifier: AGPL-3.0-or-later` | pass (grep, 2026-11-07) |
| T-S8.2 | S8.2 | Every dependency has an AGPLv3-compatible licence (list in repo) | planned |
| T-S9.2 | S9.2 | VST3 loads, plays and saves state in at least 2 hosts (e.g. Reaper, Bitwig) | planned |
| T-S10.2 | S10.2 | Under 9P load (1000 writes/s) no audio dropouts; RT thread does no syscalls (checked with RT sanitizer/trace) | planned |
| T-S10.2a | S10.2 | processBlock takes no locks and does no allocation | pass by code review 2026-11-07: keyboard notes via lock-free SPSC queues (no MidiKeyboardState call on the audio thread), MIDI decoded from raw bytes (no MidiMessage copy). Automated check (e.g. Clang RealtimeSanitizer) still to add |
| T-S10.2-spsc | S10.2 | SPSC queue: FIFO order, full/empty, wrap-around, 1M-item two-thread stress | pass (`test_spsc`; CI run 42413041183 on Ubuntu 24.04, Fedora 45, macOS arm64, Windows) |
| T-S10.1 | S10.1 | 9P2000 conformance: version/attach/walk/open/read/write/clunk/stat against the plan9port `9p` client | planned |
| T-S10.1b | S10.1 | `echo 1000 > params/vcf/cutoff` via 9P changes the audible/internal value | planned |
| T-S10.4 | S10.4 | Engine reachable over Unix socket on Linux/macOS/Windows and over TCP when enabled | planned |
| T-S10.5 | S10.5 | Two plugin instances in one host expose two distinct 9P endpoints | planned |
| T-S10.9 | S10.9 | `cat source` returns the repository URL | planned |
