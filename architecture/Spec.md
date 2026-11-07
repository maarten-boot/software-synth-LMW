# Spec — software-synth-LMW

Items marked (?) await user confirmation.

## S1 Purpose
S1.1 A polyphonic software synthesizer, inspired by AlsaModularSynth (modules + patch cables).
S1.2 Semi-modular: a default signal path is prewired ("normalled"); inserting a patch cable into an input overrides its default connection. Own design, inspired by the ARP 2600 concept (not a replica).

## S2 Platforms & language
S2.1 Runs on Linux, macOS and Windows from one codebase.
S2.2 C++20, CMake, JUCE 8.

## S3 Audio & MIDI
S3.1 Audio output via JUCE audio device layer (standalone) / host (plugin).
S3.2 Input: MIDI keyboard; computer keyboard as fallback in standalone.

## S4 Polyphony
S4.1 Voice count configurable at runtime, range 1–32.
S4.2 When all voices are busy, a new note steals the oldest voice.

## S5 Modules (initial set)
S5.1 VCO, VCF, VCA, ADSR, LFO, Mixer, Noise, MIDI-in, Audio-out.
S5.2 Default normalled patch: MIDI→VCO pitch, VCO→VCF→VCA→Out, ADSR→VCA, ADSR→VCF cutoff (? exact set)

## S6 User interface
S6.1 Simple GUI with knobs/sliders and patch points for cables.
S6.2 Default white background, black text; all colour pairs >= 4.5:1 (WCAG AA).
S6.3 Contrast adjustable interactively and via config file: presets AA (4.5:1), AAA (7:1), Max (21:1) and custom minimum ratio.

## S7 Persistence
S7.1 Save/load patches (parameters + cable connections) to a file.

## S8 Licence
S8.1 Project code is AGPLv3 (required by JUCE 8 free licence).
S8.2 Every dependency must be AGPLv3-compatible.

## S9 Deliverables
S9.1 Standalone application (first).
S9.2 VST3 plugin from the same codebase (end goal).

## S10 GUI / engine split (DRAFT ?)
S10.1 Engine and GUI communicate over 9P; engine is the 9P server and the single source of truth for state.
S10.2 The audio thread never performs socket I/O, locking or allocation; it exchanges data with the 9P thread through lock-free queues.
S10.3 Open: external GUI language (Kukel?), 9P dialect/library, transports, telemetry channel, in-plugin editor.
