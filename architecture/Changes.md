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
