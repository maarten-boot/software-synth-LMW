# Plan — software-synth-LMW

Last crosscheck with Spec and code: 2026-11-07.

| Phase | Content | Spec | Status |
|---|---|---|---|
| 0 | Repo, docs, licence, CMake + JUCE skeleton (standalone + VST3), contrast module, CI | S2, S6.2, S8 | done. CI green on Ubuntu 24.04, Fedora 45, macOS, Windows (run 42406322210). JUCE 9.0.5 and CI actions pinned by hash (S2.3). |
| 1 | Audio out + single voice (VCO→VCA, ADSR), parameters, on-screen + QWERTY keyboard | S3.1, S3.2, S5, S5a | done (unit-tested). User confirmed on Ubuntu 24.04: GUI, sound, multiple voices, double-click reset. QWERTY input fixed (worked only while the keyboard had focus) and confirmed by user. Added sustain pedal + pitch bend (S3.3, S3.4); audio thread lock-free (S10.2). |
| 2 | Voice allocator, polyphony 1–32, Voices fader | S4 | done (unit-tested); S4.2 stealing order awaiting user confirmation |
| 2a | GUI: vertical faders grouped VCO / ENV / OUT, value text with units, typed values | S6.1 | done (pulled forward from Phase 4) |
| 3 | Module graph with normalled connections + cable override; VCF, LFO, Mixer, Noise, ENV2; mod wheel; sustain glide; all parameters + two-row GUI | S1.2, S3.5, S5, S5a | done (unit-tested; GUI screenshot). Cables exist in engine/processor (lock-free queue) but can only be plugged in the GUI from Phase 4. Not yet listened to by user. |
| 4 | GUI, in this order: (1) rack: movable module panels that snap into rows, layout saved, Reset layout, keyboard/menu move; (2) patch points + cables drawn from panel sockets, following moved panels; (2b) KEYS module: POLY panel becomes KEYS with outputs keys.pitch, keys.gate, keys.vel; new inputs vco.pitch, env1.gate, env2.gate normalled from the keys (S5.8); (3) contrast settings (presets + config file); (4) theme the standalone window, Options menu and settings dialog identically to the main panel | S6.5, S6 | steps 1 (rack), 2 (cables) and 2b (KEYS) done; steps 3, 4 pending. Known limit: a host calling setStateInformation off the message thread would push cable changes from a second thread into the single-producer queue — to be handled with the 9P server (Phase 6), which adds a second producer anyway |
| 4b | More modules, before Phase 4 steps 3–4 (contrast, theming) and before patch files: (a) VCF modes — slope 6/12/24 dB and type LP/HP/BP/Notch from the ladder's stage outputs; (b) VCO 1–3 (Octave, Tune, FM, Wave each) and a 4-input mixer, ports and parameters renamed vco.* → vco1.*, with migration of saved state; default layout gets a third row; (c) VCF 2 in series after VCF 1, fully open by default (then bypassed exactly, so the default sound is unchanged) | S5.9, S5.10, S5.11 | pending |
| 5 | Patch save/load | S7 | pending |
| 6 | Own 9P2000 server in engine (non-RT thread + lock-free queues), Unix socket + TCP; audio thread free of locks/allocation | S10 | pending |
| 7 | External Kukel GUI as 9P client | S10.8 | pending (lives in the Kukel project) |
| 8 | VST3 host testing (the VST3 target already builds) | S9.2 | pending |
