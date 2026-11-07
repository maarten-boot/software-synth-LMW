# Plan — software-synth-LMW

Last crosscheck with Spec and code: 2026-11-07.

| Phase | Content | Spec | Status |
|---|---|---|---|
| 0 | Repo, docs, licence, CMake + JUCE skeleton (standalone + VST3), contrast module, CI | S2, S6.2, S8 | done. CI green on Ubuntu 24.04, Fedora 45, macOS, Windows (run 42406322210). JUCE 9.0.5 and CI actions pinned by hash (S2.3). |
| 1 | Audio out + single voice (VCO→VCA, ADSR), parameters, on-screen + QWERTY keyboard | S3.1, S3.2, S5, S5a | done (unit-tested). User confirmed on Ubuntu 24.04: GUI, sound, multiple voices, double-click reset. QWERTY input fixed (worked only while the keyboard had focus); user re-check pending. Added sustain pedal + pitch bend (S3.3, S3.4); audio thread lock-free (S10.2). |
| 2 | Voice allocator, polyphony 1–32, Voices fader | S4 | done (unit-tested); S4.2 stealing order awaiting user confirmation |
| 2a | GUI: vertical faders grouped VCO / ENV / OUT, value text with units, typed values | S6.1 | done (pulled forward from Phase 4) |
| 3 | Module graph with normalled connections + cable override; VCF, LFO, Mixer, Noise | S1.2, S5 | **next** — blocked on default patch (S5.2) |
| 4 | GUI: patch points + cables, contrast settings (presets + config file), theme the standalone window, Options menu and settings dialog identically to the main panel | S6 | pending (faders done in 2a) |
| 5 | Patch save/load | S7 | pending |
| 6 | Own 9P2000 server in engine (non-RT thread + lock-free queues), Unix socket + TCP; audio thread free of locks/allocation | S10 | pending |
| 7 | External Kukel GUI as 9P client | S10.8 | pending (lives in the Kukel project) |
| 8 | VST3 host testing (the VST3 target already builds) | S9.2 | pending |
