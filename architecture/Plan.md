# Plan — software-synth-LMW

| Phase | Content | Spec | Status |
|---|---|---|---|
| 0 | Repo, docs, licence, CMake + JUCE skeleton (standalone + VST3), contrast module, CI on 3 OSes | S2, S6.2, S8 | done on Linux; CI workflow written, not yet run (no remote) |
| 1 | Audio out + single voice (VCO→VCA, ADSR), parameters, on-screen + QWERTY keyboard | S3.1, S3.2, S5 | done on Linux (unit-tested; not yet listened to) |
| 2 | MIDI + computer keyboard, voice allocator, polyphony 1–32 | S3.2, S4 | pending |
| 3 | Module graph with normalled connections + cable override | S1.2, S5 | pending |
| 4 | In-process GUI: panel, knobs, cables, contrast settings | S6 | pending |
| 5 | Patch save/load | S7 | pending |
| 6 | Own 9P2000 server in engine (non-RT thread + lock-free queues), Unix socket + TCP | S10 | pending |
| 7 | External Kukel GUI as 9P client | S10.8 | pending |
| 8 | VST3 build target, host testing | S9.2 | pending |
