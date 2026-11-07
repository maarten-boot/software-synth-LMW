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
