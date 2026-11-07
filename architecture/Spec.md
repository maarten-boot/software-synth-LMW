# Spec — software-synth-LMW (DRAFT, pending confirmation)

Status: draft. Items marked (?) await user confirmation.

## S1 Purpose
S1.1 A simple software synthesizer inspired by AlsaModularSynth (AMS): modules connected by patch cables.

## S2 Platforms
S2.1 Runs on Linux, macOS and Windows from one codebase.
S2.2 Language: C++ (?) — C++17/20.

## S3 Audio & MIDI
S3.1 Cross-platform audio output (?) e.g. miniaudio.
S3.2 MIDI keyboard input (?) e.g. RtMidi; plus on-screen/computer-keyboard playing (?).

## S4 Polyphony
S4.1 Number of voices is configurable at runtime (?) range 1–32.
S4.2 Voice allocation with voice stealing when all voices are busy (?).

## S5 Modules (?)
S5.1 Initial set: VCO, VCF, VCA, ADSR, LFO, Mixer, MIDI-in, Audio-out.

## S6 User interface
S6.1 Simple GUI (?) e.g. Dear ImGui with a node editor for patching.
S6.2 Default white background, black text; all colour pairs >= 4.5:1 (WCAG AA).
S6.3 Contrast adjustable interactively and via config file: presets AA (4.5:1), AAA (7:1), Max (21:1) and custom minimum ratio.

## S7 Persistence (?)
S7.1 Save/load patches to a file.
