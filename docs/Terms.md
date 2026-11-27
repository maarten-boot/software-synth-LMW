# LMW-Synth terms

The words LMW-Synth uses, in plain English, by topic. Each term says where you find it in the
program and which other terms go with it. In the program: Menu › Terms... (with a Find field).

## Basics

### Synthesizer
An instrument that makes its sound electronically from simple waves, which you then shape: filter
them, give them a start and an end, let them move. LMW-Synth works like the classic analogue
synthesizers, in software.

Where: the whole program.

See also: [Module](#module), [Patch](#patch), [Voice](#voice)

### Semi-modular
Everything is already connected in a sensible way, so it plays at once; with cables you change or
add connections. A fully modular synthesizer plays nothing until you patch it.

Where: the Synth, Matrix, Schematic and Effects tabs.

See also: [Normalled](#normalled), [Cable](#cable), [Patch](#patch)

### Module
One building block with one job, such as a VCO, a VCF or an envelope. Each module is a panel with
faders and sockets.

Where: the panels on the Synth and Effects tabs; the blocks on the Schematic tab.

See also: [Panel](#panel), [Socket](#socket), [Rack](#rack)

### Panel
The box of one module on the Synth or Effects tab: its title bar (in the module's colour), its faders
and choices, and its sockets at the bottom. Drag the title bar to move the panel.

Where: Synth and Effects tabs.

See also: [Module](#module), [Rack](#rack), [Fader](#fader)

### Fader
A sliding control (vertical on the panels) that sets one value. Drag it, use the arrow keys, or type
a value in the box below it; double-click for its default.

Where: every panel.

See also: [Scale](#scale), [Undo](#undo)

### Voice
One complete copy of the sound engine, playing one note. With 8 voices you can play 8 notes at once;
each voice has its own VCOs, filters and envelopes.

Where: KEYS › Voices.

See also: [Polyphony](#polyphony), [Note](#note), [Load](#load)

### Load
How much of the computer's time the synth needs: "CPU 7 %" in the header is the share of the time each
block of sound may take. Above 70 %, or after a dropout (a block that took too long: a click or a gap),
it turns red. More voices and effects cost more; its hover text also shows the voices playing and the
memory used.

Where: the header, left of Undo.

See also: [Voice](#voice), [Polyphony](#polyphony)

### Polyphony
Playing several notes at once. LMW-Synth has up to 32 voices; when all are in use, a new note takes
one of them over.

Where: KEYS › Voices.

See also: [Voice](#voice)

### Patch
The whole sound: every fader and choice, the cables, the notes you wrote and, when you moved them,
the Schematic positions. Also the file it is saved in.

Where: header › Save..., Load...; Patch: <name> in the header.

See also: [Patch file](#patch-file), [Instrument preset](#instrument-preset), [Cable](#cable)

### Preset
A ready-made setting you can choose from a menu: a whole instrument (header › Presets) or one
module (the Presets button in a panel's title bar).

Where: header › Presets; Presets in the title bars.

See also: [Instrument preset](#instrument-preset), [Module preset](#module-preset)

## Sound sources

### VCO
Voltage-controlled oscillator: makes the raw tone, a wave repeating at the note's pitch. LMW-Synth
has three per voice (VCO 1–3), mixed in the MIXER.

Where: the VCO 1, VCO 2, VCO 3 panels.

See also: [Waveform](#waveform), [Tune](#tune), [Octave](#octave), [FM](#fm), [PM](#pm), [Mixer](#mixer)

### Waveform
The shape of one cycle of a wave; it sets the basic colour of the tone. Sine is pure, triangle soft,
square hollow (clarinet-like), saw bright (strings, brass), pulse thin and nasal.

Where: a VCO's Wave choice; an LFO's Wave choice.

See also: [VCO](#vco), [LFO](#lfo)

### Octave
Twelve semitones: the same note, twice or half the pitch. A VCO's Oct choice moves it −2 … +2
octaves.

Where: VCO › Oct.

See also: [Tune](#tune), [Semitone](#semitone)

### Semitone
The step between two neighbouring keys on the keyboard (twelve in an octave). The VCO Tune and FM
faders count in semitones (st).

Where: VCO › Tune, VCO › FM.

See also: [Tune](#tune), [Octave](#octave)

### Tune
Shifts a VCO up or down a little, up to ±7 semitones. Two VCOs a fraction apart (e.g. +0.1 and
−0.08 st) beat against each other and sound wide: the classic "detune".

Where: VCO › Tune.

See also: [Semitone](#semitone), [VCO](#vco)

### Noise
A sound without pitch: hiss. White noise is bright, pink softer, brown dull. Good for breath, wind,
drums. The NOISE module also gives random steps (Rand).

Where: the NOISE panel; MIXER › Noise.

See also: [Random](#random), [Mixer](#mixer)

### Random
NOISE's Rand output: a new random value at Rate per second, held in between (or smoothed with
Smooth). Patched to a filter it gives the "sample and hold" bleeps.

Where: NOISE › Rate, Smooth, Rand socket.

See also: [Noise](#noise), [Modulation](#modulation)

### FM
Frequency modulation: one signal moves a VCO's pitch up and down. Slow and small it is vibrato; fast
and large it makes bell-like, metallic tones. A VCO's FM fader sets how far (in semitones); LFO 1 is
connected to it from the start.

Where: VCO › FM fader and socket.

See also: [PM](#pm), [Vibrato](#vibrato), [LFO](#lfo)

### PM
Phase modulation: one signal moves a VCO's phase instead of its pitch. With another VCO as the source
it gives the clear, glassy tones of electric pianos and bells.

Where: VCO › PM fader and socket.

See also: [FM](#fm), [VCO](#vco)

### Mixer
Adds the three VCOs and the noise together, each at its own level, before the filters.

Where: the MIXER panel.

See also: [VCO](#vco), [Noise](#noise), [VCF](#vcf), [MIX 2](#mix-2)

### MIX 2
A second mixer after the filters (and FORMANT), with a level and a pan for each of its three inputs. In 1
gets FORMANT; In 2 and In 3 take cables, e.g. VCF 1 left and VCF 2 right, so the VCOs of one voice can sit
apart. While an input is off the centre the voice is stereo from here through the VCA to OUT.

Where: the MIX 2 panel; Presets › Choral flute uses it.

See also: [Mixer](#mixer), [Pan](#pan), [Stereo](#stereo)

## Filters

### VCF
Voltage-controlled filter: takes away part of the sound — usually the highs, making it darker. It is
the main tool to shape the tone. LMW-Synth has two (VCF 1 and VCF 2), in a row or side by side.

Where: the VCF 1 and VCF 2 panels.

See also: [Cutoff](#cutoff), [Resonance](#resonance), [Slope](#slope), [VCF routing](#vcf-routing)

### Cutoff
The frequency where a filter starts to work. A lowpass filter with a low cutoff sounds dark and
muffled; moving the cutoff while a note plays is the classic "wah" sweep.

Where: VCF › Cutoff; the Cut socket.

See also: [VCF](#vcf), [Resonance](#resonance), [Envelope](#envelope), [Key tracking](#key-tracking)

### Resonance
Makes a filter ring at its cutoff: a peak that sounds nasal or whistling; above about 91 % the filter
sings on its own — a sine at the cutoff, even with nothing at its input (set MIXER's levels to 0 and
play: Key 100 % makes it follow the keys). Its own output patched back into its input lowers the
resonance (at 100 % Reso a loop above about 40 % stops it).

Where: VCF › Reso; the Res socket.

See also: [Cutoff](#cutoff), [VCF](#vcf)

### Slope
How steeply a filter takes away what it removes: 24 dB per octave is steep and smooth, 12 softer, 6
gentle; 36 dB is the steepest and darkest, with a softer resonance.

Where: VCF › Slope.

See also: [dB](#db), [VCF](#vcf)

### Filter type
Which part a filter keeps: Lowpass keeps the lows (darker), Highpass the highs (thinner), Bandpass a
band around the cutoff (telephone-like), Notch everything except a band.

Where: VCF › Type.

See also: [VCF](#vcf), [Cutoff](#cutoff)

### Key tracking
The filter's cutoff follows the notes you play: higher notes open it, so they keep their brightness.

Where: VCF › Key.

See also: [Cutoff](#cutoff), [Keyboard](#keyboard)

### VCF routing
How the two filters are connected: Serial (VCF 1 then VCF 2) or Parallel (both get the mixer, their
outputs are mixed).

Where: VCF 2 › the Serial / Parallel choice.

See also: [VCF](#vcf), [Normalled](#normalled)

### Formant
The peaks in a sound's spectrum that make a vowel: our ears hear "a", "e", "o" from where they lie.
The FORMANT module filters the sound into a sung vowel.

Where: the FORMANT panel (Voice, Vowel, Reso, Mix).

See also: [Vowel](#vowel), [VCF](#vcf)

### Vowel
The sung vowel the FORMANT module makes: A, E, I, O or U, gliding in between; the Vowel socket lets an
LFO or envelope move it.

Where: FORMANT › Vowel.

See also: [Formant](#formant), [Modulation](#modulation)

### dB
Decibel: a step in loudness. 6 dB is about twice (or half) the level; −inf is silence.

Where: OUT › Volume; VCF › Slope (dB per octave).

See also: [Slope](#slope), [Volume](#volume)

## Shaping over time

### Envelope
A shape that runs once per note: it rises when the key goes down and falls when it is released.
ENV 1 shapes the loudness (into the VCA), ENV 2 the filter.

Where: the ENV 1 → VCA and ENV 2 → VCF panels.

See also: [ADSR](#adsr), [VCA](#vca), [Cutoff](#cutoff), [Gate](#gate)

### ADSR
The four stages of an envelope. Attack: the time to rise; Decay: the time to fall to the Sustain
level, which holds while the key is down; Release: the time to fade after the key is let go.

Where: ENV › Attack, Decay, Sustain, Release.

See also: [Envelope](#envelope)

### Gate
An on/off signal: on while a key is held. It starts and stops the envelopes. Gate sockets have a
dashed ring (magenta).

Where: KEYS › Gate socket; ENV › Gate socket.

See also: [Envelope](#envelope), [Note](#note), [Socket](#socket)

### LFO
Low-frequency oscillator: a slow wave (below what you hear as a tone) that moves something else.
LFO 1 gives vibrato (pitch), LFO 2 tremolo (loudness). Its Delay lets it fade in after each note starts.

Where: the LFO 1 and LFO 2 panels.

See also: [Vibrato](#vibrato), [Tremolo](#tremolo), [Rate](#rate), [Delayed vibrato](#delayed-vibrato), [Modulation](#modulation)

### Rate
How fast an LFO (or the NOISE random steps) runs, in Hz: cycles per second.

Where: LFO › Rate; NOISE › Rate; the Rate sockets.

See also: [LFO](#lfo), [Hz](#hz)

### Hz
Hertz: times per second. A pitch of 440 Hz is the A above middle C; an LFO at 5 Hz wobbles five
times a second.

Where: VCF › Cutoff, LFO › Rate and other faders.

See also: [Rate](#rate), [Cutoff](#cutoff)

### Vibrato
A gentle, regular wobble of the pitch, as singers and violinists use. Here: LFO 1 on the VCOs' FM;
the FM fader sets how far.

Where: LFO 1; VCO › FM; the mod wheel adds more.

See also: [LFO](#lfo), [FM](#fm), [Mod wheel](#mod-wheel), [Delayed vibrato](#delayed-vibrato)

### Delayed vibrato
Vibrato that starts a moment after the note, as singers and violinists play it: the note begins
plain, then the wobble fades in. LFO 1 › Delay sets when it is at full depth (nothing for the first
half of that time, then a smooth fade-in); 0 = at once. LFO 2 has the same for tremolo. It counts per
note and works wherever the LFO goes into a voice (VCOs, VCFs, VCA); a cable from the LFO straight to OUT
or to an LFO's Rate input is not delayed (the LFO is used once there, for all notes).

Where: LFO 1 › Delay (LFO 2 › Delay for tremolo).

See also: [Vibrato](#vibrato), [LFO](#lfo), [Tremolo](#tremolo)

### Tremolo
A regular wobble of the loudness. Here: LFO 2 on the VCA's Mod input; VCA › Tremolo sets how deep.

Where: LFO 2; VCA › Tremolo.

See also: [LFO](#lfo), [VCA](#vca)

### VCA
Voltage-controlled amplifier: sets how loud each voice is, normally following ENV 1, so notes start
and end. Initial adds a level without any envelope, Velocity how much the key velocity counts.

Where: the VCA panel.

See also: [Envelope](#envelope), [Velocity](#velocity), [Tremolo](#tremolo)

### Velocity
How hard (fast) a key is struck, from a MIDI keyboard. It sets the loudness through the VCA (Velocity
fader) and can be patched elsewhere from KEYS › Vel.

Where: VCA › Velocity and Vel socket; KEYS › Vel.

See also: [VCA](#vca), [MIDI](#midi)

## Patching

### Socket
A round connection point at the bottom of a panel. White sockets are inputs, black ones outputs; a
plugged input has a black centre. The ring around it tells the type of signal: a solid ring (blue) for
audio, a double ring (orange) for a control voltage, a dashed ring (magenta) for a gate. Any output can
still go to any input; the ring says what the input is made for, and its hover text names the type.
Menu › Show socket rings switches the rings off (the hover text keeps the type).

Where: the bottom row of every panel; the ports on the Schematic (its legend shows the rings).

See also: [Input](#input), [Output](#output), [Cable](#cable), [Audio signal](#audio-signal), [Control voltage (CV)](#control-voltage-cv), [Gate](#gate)

### Stereo
Sound with a left and a right side. In LMW-Synth every voice is mono until OUT, which gives each voice
its own place left–right (Pan, Spread) before the voices are added; the effects then work in stereo.

Where: OUT › Pan and Spread; the Effects tab.

See also: [Pan](#pan), [Spread](#spread), [Effect](#effect)

### Pan
Where a sound sits between left and right: −100 % left, 0 the centre, +100 % right. Equal power: a
voice moved to one side is as loud as in the centre. The Pan input moves each voice by a signal, e.g.
LFO 2 for auto-pan or noise.rand for a random place per note.

Where: OUT › Pan and its Pan socket.

See also: [Spread](#spread), [Stereo](#stereo), [LFO](#lfo)

### Spread
How far the notes spread around Pan. Keys: by pitch, as on a piano (C4 in the centre, low notes left,
high notes right). Voices: by voice, alternating left and right; with one voice the note stays in the
centre, so a solo sound spreads only when you give it more voices.

Where: OUT › Spread and the box under it.

See also: [Pan](#pan), [Voice](#voice), [Stereo](#stereo)

### Audio signal
A signal you can hear: the sound itself, from a VCO, NOISE, the mixer, a filter, FORMANT, the VCA, OUT
or an effect. Audio sockets have a solid ring (blue).

Where: the sockets with a solid ring; "audio" in 9P /ports.

See also: [Control voltage (CV)](#control-voltage-cv), [Gate](#gate), [Socket](#socket)

### Control voltage (CV)
A slower signal that moves a setting instead of being heard: an LFO, an envelope, the key pitch and
velocity, noise.rand. CV sockets have a double ring (orange); a modulation input takes it at a level
from −100 to +100 %. The FM and PM inputs of the VCOs are CV inputs that also take audio (a VCO on
another VCO's FM or PM gives new overtones).

Where: the sockets with a double ring; "cv" in 9P /ports ("cv+audio" for FM and PM).

See also: [Modulation](#modulation), [Audio signal](#audio-signal), [Gate](#gate), [PM](#pm)

### Input
A socket where a signal goes in: it changes something, e.g. VCF › Cut moves the cutoff.

Where: the white sockets.

See also: [Output](#output), [Socket](#socket)

### Output
A socket where a signal comes out, e.g. LFO 1 › Out or ENV 2 › Out.

Where: the black sockets.

See also: [Input](#input), [Socket](#socket)

### Cable
A connection you make from an output to an input (drag from one socket to another, or click in the
Matrix). A cable replaces the input's normal connection; up to 4 cables on one input add up.

Where: the Synth, Matrix, Schematic and Effects tabs.

See also: [Normalled](#normalled), [Level](#level), [Matrix](#matrix)

### Normalled
Connected from the start, without a cable: e.g. ENV 2 → VCF 1 cutoff. Patching a cable into the input
replaces it; removing the cable brings it back.

Where: the dashed lines on the Schematic, the rings in the Matrix.

See also: [Cable](#cable), [Semi-modular](#semi-modular), [No effect](#no-effect)

### No effect
A unit whose output does not reach MAIN, or only through a level of 0 (e.g. VCO 2 while MIXER's In 2 is
at 0), changes nothing in the sound. Its outline is drawn with short dashes and the lines from it dotted;
FORMANT or an effect at Mix 0 passes the sound unchanged: long dashes. A unit that no sound reaches
(e.g. VCF 1 with only its own output patched into its input) is silent, and so is everything after it.
Its hover text says why.

Where: the Schematic, the Synth and Effects tabs.

See also: [Normalled](#normalled), [Level](#level), [Mix](#mix)

### Level
How much of a cable's signal arrives, 0–100 % (−100 … +100 % on modulation inputs). Set it in a
socket's menu (right-click), with the Matrix slider, or over 9P.

Where: right-click a socket › Levels; the Matrix.

See also: [Cable](#cable), [Inverted](#inverted)

### Inverted
A cable at a negative level: the source upside down. An inverted envelope on the cutoff closes the
filter as the envelope rises.

Where: modulation inputs, levels below 0.

See also: [Level](#level), [Modulation](#modulation)

### Modulation
One signal changing a setting over time: an LFO moving the pitch, an envelope moving the cutoff.
Modulation inputs (pitch, FM, PM, cutoff, resonance, vowel, VCA CV and Mod, LFO rates) take negative
levels too.

Where: the inputs under the faders.

See also: [LFO](#lfo), [Envelope](#envelope), [Inverted](#inverted)

### Matrix
A grid of every input (rows) and every output (columns), like the pin matrix of the EMS VCS3: a
filled cell is a cable with its level, a ring a normal connection. The synth's inputs and outputs are
on the Matrix tab; the effects' (COMB, ENSEMBLE, DELAY, REVERB, MAIN) in a small matrix of their own on
the Effects tab, in the effects' order.

Where: the Matrix tab; the Effects tab (the effects).

See also: [Cable](#cable), [Normalled](#normalled), [Level](#level)

### Schematic
The whole instrument drawn as a diagram, the sound flowing left to right: every module a block, every
connection a line. You can patch and move blocks there too.

It is drawn at the Synth tab's text size; Ctrl + wheel or Ctrl + / − zooms (Ctrl 0: 100 %). Drag on empty space (or Ctrl / Shift-click, Ctrl + A) to select several symbols and drag one of them to move them all.
Where: the Schematic tab.

See also: [Module](#module), [Cable](#cable)

### Rack
The rows the panels sit in, as in a studio rack; the lines between rows are the rack rails. Drag a
panel's title to move it. A rack larger than the window scrolls: the scroll bars, the mouse wheel
(Shift + wheel sideways), dragging the empty grey space, Home / End; Tab brings a control into view.

Where: the Synth tab; Reset layout.

See also: [Panel](#panel), [Compact](#compact)

## Effects

### Effect
A stage after the voices that changes the whole sound: COMB, ENSEMBLE, DELAY and REVERB, in that
order from SYNTH to MAIN; each is off while its Mix is 0.

Where: the Effects tab.

See also: [Mix](#mix), [COMB](#comb), [ENSEMBLE](#ensemble), [DELAY](#delay), [REVERB](#reverb)

### Mix
Dry / wet: how much of an effect you hear. 0 = off (the sound passes unchanged), 100 % = only the
effect.

Where: each effect's Mix fader; FORMANT › Mix.

See also: [Effect](#effect)

### COMB
A comb filter: many short resonances at multiples of one frequency, a hollow, tube- or body-like
colour; with Key it follows the notes (wind instruments, plucked strings).

Where: Effects tab › COMB.

See also: [Feedback](#feedback), [Effect](#effect)

### ENSEMBLE
A chorus like the string machines of the 1970s (the Solina): three slightly delayed copies whose
delays keep moving, spread left and right, so a plain saw sound becomes a wide string section.

Where: Effects tab › ENSEMBLE (Depth, Rate, Shimmer, Mix).

See also: [Chorus](#chorus), [Effect](#effect)

### Chorus
Copies of a sound slightly delayed and gently moving in pitch, mixed with the original: it sounds
like several players at once, wider and livelier.

Where: Effects tab › ENSEMBLE.

See also: [ENSEMBLE](#ensemble)

### DELAY
Echoes: the sound repeated after Time, each repeat (Feedback) a little darker (Damping); Spread lets
them bounce between left and right.

Where: Effects tab › DELAY.

See also: [Feedback](#feedback), [Effect](#effect)

### REVERB
The sound of a room: many reflections blending into a tail. Size sets the room, Damping how dark,
Width how wide, Pre-delay the gap before the tail starts (clarity for voices and solo instruments).
Type: Hall (a smooth, dense tail; new sounds) or Classic (the reverb of version 1.7 and before; older
patches keep it, so they sound as they did).

Where: Effects tab › REVERB.

See also: [Effect](#effect)

### Feedback
Part of an effect's output fed back into it, so it repeats or rings longer (DELAY: more echoes; COMB:
stronger resonances).

Where: DELAY › Feedback; COMB › Feedback.

See also: [DELAY](#delay), [COMB](#comb)

## Playing

### Keyboard
The on-screen piano at the bottom (A0 to C8). The computer keys play too: A W S E D F T G Y H U J K
from middle C. Ctrl+K hides it.

Where: the bottom of the window; Menu › Show keyboard.

See also: [Note](#note), [MIDI](#midi)

### Note
One key played: a pitch with a start (key down) and an end (key up), played by one voice.

Where: the keyboard, a MIDI keyboard, 9P /keys.

See also: [Voice](#voice), [Gate](#gate), [Velocity](#velocity)

### MIDI
The standard way music keyboards and computers send notes and controls. Connect a MIDI keyboard in
Options › Audio/MIDI Settings...; in a DAW it comes from the track.

Where: Options › Audio/MIDI Settings...

See also: [Keyboard](#keyboard), [Mod wheel](#mod-wheel), [Velocity](#velocity)

### Mod wheel
The wheel next to a MIDI keyboard's keys: here it adds vibrato (more FM depth on every VCO).
 The Mod wheel left of the keyboard does the same; KEYS › Mod is its output, to patch it anywhere.
Where: a MIDI keyboard; the VCO hover texts.

See also: [Vibrato](#vibrato), [FM](#fm)


### Glide
Portamento: a new note slides in from the pitch of the last one instead of jumping. Glide sets how long
the slide takes (0 = none); Always slides on every note, Legato only while another key is still held —
play smoothly to slide, detached to jump.

Where: left of the keyboard (Glide and its box).

See also: [Note](#note), [Pitch bend](#pitch-bend), [Voice](#voice)
### Pitch bend
The other wheel on a MIDI keyboard: bends the pitch of the notes up or down.
 The Bend wheel left of the keyboard does the same and springs back; the Range box sets how far (1, 2, 7 or 12 semitones).
Where: a MIDI keyboard.

See also: [MIDI](#midi)

### Sustain pedal
A pedal on a MIDI keyboard that holds the notes after the keys are released, as on a piano.

Where: a MIDI keyboard.

See also: [MIDI](#midi), [Envelope](#envelope)

### Volume
The loudness of the whole synth, before the effects.

Where: OUT › Volume.

See also: [dB](#db)

## Files and control

### Patch file
A .lmwpatch text file with a whole sound (values, cables, notes, Schematic positions). You can open it
in any text editor.

Where: header › Save..., Load...; Documents/LMW-Synth Patches.

See also: [Patch](#patch)

### Instrument preset
A whole sound in the header's Presets menu: the built-in instruments (Flute, String section, ...) and
your own (Save as instrument preset...).

Where: header › Presets.

See also: [Preset](#preset), [Module preset](#module-preset)

### Module preset
The settings of one module, from the Presets button in its title bar; the title bar shows its name
("Oboe", with * once you change it).

Where: the Presets button of a panel.

See also: [Preset](#preset), [Instrument preset](#instrument-preset)

### Theme
The colours of the panels, cables and highlight: Default, Soft, Vivid, Monochrome or your own.

Where: Menu › Theme.

See also: [Contrast](#contrast), [Highlight](#highlight)

### Undo
Takes back the last change to the sound (a fader drag, a cable, a preset); Redo does it again. The
last 100 changes of this session.

Where: the ↶ Undo and Redo ↷ buttons; Ctrl+Z, Ctrl+Shift+Z.

See also: [Patch](#patch)

### 9P
A simple file protocol: every parameter, the cables, the notes and the keys are files that other
programs can read and write.

Where: hover the title LMW-Synth for the address; the lmw9p tool.

See also: [lmw-mcp](#lmw-mcp)

### lmw-mcp
A small program that lets an AI assistant you already use (Claude Desktop, Claude Code) set up
sounds through 9P.

Where: in every download; README.

See also: [9P](#9p), [Assistant](#assistant)

### Assistant
The Assistant tab: describe a sound in your own words and Claude sets it up and plays it. Needs a
Claude API key.

Where: the Assistant tab; Menu › Assistant settings...

See also: [lmw-mcp](#lmw-mcp)

## Display

### Contrast
How strongly colours stand out against black text: AA 4.5:1 (default), AAA 7:1, Max 21:1 (only black
and white), or your own ratio.

Where: Menu › Contrast.

See also: [Theme](#theme)

### Compact
A smaller view: narrower panels, shorter faders, a smaller Schematic; the window keeps its size.

Where: header › Compact.

See also: [Rack](#rack)

### Highlight
The golden-yellow box around the socket under the mouse, with keyboard focus or under a dragged
cable.

Where: the sockets; Menu › Theme › Edit colours....

See also: [Socket](#socket), [Theme](#theme)

### Scale
How a fader's travel maps to its value, shown by its tag and ticks: lin (even steps), ± (around 0),
log (frequencies and rates), curve (more travel at small values, e.g. times). Menu › Show fader scales
switches the ticks and tags off (the hover text keeps the line).

Where: under each fader's name; the hover text; Menu › Show fader scales.

See also: [Fader](#fader)
