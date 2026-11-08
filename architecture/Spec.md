# Spec — software-synth-LMW

Items marked (?) await user confirmation.

## S1 Purpose
S1.1 A polyphonic software synthesizer, inspired by AlsaModularSynth (modules + patch cables).
S1.2 Semi-modular: a default signal path is prewired ("normalled"); inserting a patch cable into an input overrides its default connection. Own design, inspired by the ARP 2600 concept (not a replica).

## S2 Platforms & language
S2.1 Runs on Linux, macOS and Windows from one codebase. Linux targets: Ubuntu 24.04 LTS and newer, and the latest Fedora release (currently 45). Linux binaries are built on the oldest supported distro so they also run on newer ones.
S2.2 C++20, CMake, JUCE 9.
S2.3 CI and other external build steps (GitHub Actions `uses:` etc.) are pinned to a full commit SHA with the version as a comment, using the latest workable version; any exception is documented with its reason.

## S3 Audio & MIDI
S3.1 Audio output via JUCE audio device layer (standalone) / host (plugin).
S3.2 Input: MIDI keyboard; computer keyboard as fallback in standalone. Computer-keyboard notes (keys a w s e d f t g y h u j k o l p ;, starting at C4) work whichever control has focus, except while typing into a value box.
S3.3 Sustain pedal (CC64): while down, released keys keep sounding; pedal up releases them. Re-striking a sustained note retriggers it (no doubling).
S3.4 Pitch bend: applies to all sounding and new voices; range ±2 semitones (fixed for now; may become a parameter).
S3.5 Mod wheel (CC1): adds vibrato — the FM depth becomes vco.fm + wheel × 1 semitone (the LFO always reaches vco.fm; changed 2026-11-08 so FM also works without a MIDI controller).

## S4 Polyphony
S4.1 Voice count configurable at runtime, range 1–32.
S4.2 When all voices are busy, a new note steals the oldest voice, preferring voices already in release over held ones.
S4.3 A note-on for a note that is already sounding retriggers that voice (no doubling).
S4.4 Stealing/retriggering keeps phase and envelope level continuous (no click).
S4.5 Lowering the voice count releases the excess voices (they ring out); parameter id voice.count, default 8.
S4.6 The summed output passes a soft limiter: unchanged below -6 dBFS, smooth (tanh) knee above, never above -0.04 dBFS — no hard clipping with many voices.

## S5 Modules (initial set)
S5.1 VCO, VCF, VCA, ADSR, LFO, Mixer, Noise, MIDI-in, Audio-out. VCF: 24 dB/oct lowpass ladder with resonance. LFO: one shared (global) LFO for all voices.
S5.2 Default normalled patch (confirmed 2026-11-07): MIDI→VCO pitch; VCO→Mixer→VCF→VCA→Out; Noise→Mixer (level 0); ENV1→VCA; ENV2→VCF cutoff; LFO→VCO pitch with depth set by the mod wheel (0 by default).
S5.3 Ports. Outputs: vco.out, noise.out, lfo.out, env1.out, env2.out, mix.out, vcf.out, vca.out. Inputs, with their normalled source: vco.fm ← lfo.out; mix.in1 ← vco.out; mix.in2 ← noise.out; vcf.in ← mix.out; vcf.cutoff ← env2.out; vca.in ← vcf.out; vca.cv ← env1.out; out.in ← vca.out.
S5.4 Cables. A cable connects one output to one input; an output may feed several inputs; an input takes at most one cable. A cable replaces the input's normalled source (it does not add to it); removing it restores the normalled source. Input amounts (vco.fm, mixer levels, vcf.env) still apply to a patched signal. A cable from a module computed later in the same sample (feedback) reads that output's previous sample. Cable changes reach the audio thread through a lock-free queue (S10.2).
S5.5 Signal levels: audio ±1; envelopes 0..1; LFO ±1; noise white, uniform ±1. vco.fm: 1.0 in = (vco.fm + mod wheel × 1) semitones. vcf.cutoff: 1.0 in = vcf.env octaves. VCA gain = vca.initial + vca.cv, clamped to 0..1. out.in: per-voice signals are summed; a global source (lfo, noise) is output once.
S5.6 Shared (global) modules: LFO, Noise. Per voice: VCO, ENV1, ENV2, Mixer, VCF, VCA. When the sound no longer ends with ENV1 (out.in or vca.cv patched, or vca.initial > 0), voices that have played keep running after release (drone), as on a modular.
S5.7 VCF: 24 dB/oct lowpass ladder (zero-delay-feedback, 4 one-pole stages, tanh-limited feedback), resonance 0..1 (self-oscillates near 1); cutoff 20 Hz–18 kHz, smoothed 20 ms in the log domain.

## S5a Parameters
S5a.1 Parameter IDs are dotted paths that map 1:1 to 9P files (default in brackets): voice.count 1–32 [8]; vco.wave Sine/Saw/Square/Triangle [Saw]; vco.fm 0–12 st [0]; mix.vco 0–1 [1]; mix.noise 0–1 [0]; vcf.cutoff 20 Hz–18 kHz [18 kHz]; vcf.res 0–1 [0]; vcf.env −5…+5 oct [0]; vca.initial 0–1 [0]; env1.attack 0–5 s [10 ms]; env1.decay 0–5 s [200 ms]; env1.sustain 0–1 [0.7]; env1.release 0–10 s [300 ms]; env2.* same ranges [10 ms, 300 ms, 0, 300 ms]; lfo.rate 0.05–20 Hz [5]; lfo.wave Sine/Triangle/Saw/Square [Sine]; out.gain 0–1 [0.8]. With these defaults the sound equals Phase 2 (filter open, no noise, no vibrato). Phase-2 ids env.* were renamed env1.* (old saved state loses the envelope values).
S5a.2 MIDI events are applied sample-accurately within an audio block.
S5a.3 Parameter changes act on sounding notes: volume immediately (20 ms smoothing); attack/decay times change the running stage; release time is taken at key release; a sustain-level change on a held note glides over 20 ms (no click). Filter cutoff is smoothed (S5.7); other parameters apply at the next block.

## S6 User interface
S6.1 Simple GUI with vertical sliders (faders, no rotary knobs) grouped per module in two rows — POLY (Voices) | VCO | MIXER | VCF | VCA | LFO | OUT, then ENV 1 → VCA | ENV 2 → VCF (titles show the normalled destination). Hovering a fader for 2 s shows a text with the input it acts on, its current source (normalled or cable) and what it does (e.g. FM shows FM + mod wheel = depth now), each with a value box showing units (ms/s, %, dB), and patch points for cables. Values can be typed into the box; double-click on a fader resets it to its default.
S6.2 Default white background, black text; all colour pairs >= 4.5:1 (WCAG AA). This covers every window, including the standalone title bar, its Options menu and the audio/MIDI settings dialog: they look identical to the main panel.
S6.3 Contrast adjustable interactively and via config file: presets AA (4.5:1), AAA (7:1), Max (21:1) and custom minimum ratio.
S6.4 Compact view: a "Compact" toggle in the header switches to narrower columns, shorter faders, smaller labels and value boxes (window 774×496 instead of 1046×658). The choice is stored with the saved state (later also in the config file, with S6.3).
S6.5 Rack layout: each module panel (POLY, VCO, MIXER, VCF, VCA, LFO, OUT, ENV 1, ENV 2) can be moved by dragging its title bar; it snaps into a row position (within or between rows) with a drop marker, and the other panels move aside. The same moves are possible without a mouse (keyboard / menu: move left/right/up/down). The layout is stored with the saved state (later also in the config file) and a "Reset layout" control restores the default. Cables (Phase 4 step 2) follow the panels. Floating/undocked module windows are out of scope.

## S7 Persistence
S7.1 Save/load patches (parameters + cable connections) to a file.

## S8 Licence
S8.1 Project code is licensed AGPL-3.0-or-later (SPDX header in every source file). JUCE 9 is used under its AGPLv3 option (JUCE is dual AGPLv3/commercial), so the built app/plugin is distributed under AGPLv3.
S8.2 Every dependency must be AGPLv3-compatible.

## S9 Deliverables
S9.0 Product name: `LMW-Synth` (no space) for the app file, VST3 bundle, window title and plugin name.
S9.1 Standalone application (first).
S9.2 VST3 plugin from the same codebase (end goal).

## S10 GUI / engine split
S10.1 Engine and GUI communicate over 9P2000; the engine is the 9P server and the single source of truth for state.
S10.2 The audio thread never performs socket I/O, locking or allocation; it exchanges data with the 9P thread through lock-free SPSC queues.
S10.3 The 9P2000 server is implemented in this project (C++20, no external 9P library).
S10.4 Transports: Unix domain socket (default, all 3 OSes; Windows 10+) and optional TCP.
S10.5 Each engine instance (standalone or each plugin instance) gets a unique socket path/port.
S10.6 Telemetry (meters, scope) is served as 9P files read repeatedly by the client; ZeroMQ deferred until measurements show a need.
S10.7 Plugin and standalone keep a built-in JUCE GUI; the external GUI is an additional 9P client.
S10.8 External GUI is written in Kukel (MIT), runs as a separate process, and lives in the Kukel project (not this repo).
S10.9 Engine exposes /source (URL of source code) to satisfy AGPLv3 §13 for remote users.
