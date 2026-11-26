# LMW-Synth sound design guide (for AI assistants)

This guide is served by `lmw-mcp` as `lmw://guide/sound-design` (Spec S11.5). It tells an assistant how
LMW-Synth makes sound, so it can turn a description ("a male ooo voice", "a soft pad") into settings.
Use the tools: `describe_synth` for exact ranges, `set_parameters` and `set_cables` to change the sound,
`play` to listen, `get_sound` to check. Start from a preset when one is close (`list_presets`,
`apply_instrument_preset`), then adjust. Values are written as in patch files: numbers in the
parameter's own unit (seconds, Hz, octaves, semitones; 0-1 for percentages), choices by name.
`describe_synth` also lists every socket with its type: audio (sound), cv (a control voltage: LFO,
envelope, key pitch / velocity) or gate (on / off); "cv+audio" inputs (VCO FM and PM) take audio too.
Patch audio into audio inputs and CV into modulation inputs unless you want an effect such as audio-rate FM.
Stereo: `out.pan` -1..1 [0] (centre), `out.spread` 0-1 [0] with `out.spreadmode` Keys (by pitch, piano-like) or
Voices (alternating; one voice stays centred); a cable into `out.pan` (e.g. lfo2.out at 50 %) pans each voice.
Width before the effects makes ENSEMBLE, DELAY and REVERB sound wider; keep solo instruments centred.
World presets (`list_presets`, `apply_instrument_preset`): Shakuhachi, Bansuri, Pan flute, Duduk, Erhu, Shamisen, Koto,
Sitar, Oud, Kalimba, Gamelan, Steel pan, Didgeridoo — good starting points for non-western timbres.
MIX 2 (after FORMANT, before the VCA): `mix2.level1-3` [1, 0, 0], `mix2.pan1-3` -1..1 [0]; `mix2.in1` is
normalled from formant.out, in2 / in3 take cables. Panning an input makes the voice stereo, e.g. cables
vcf1.in=vco1.out, vcf2.in=vco2.out, mix2.in1=vcf1.out (pan -0.7), mix2.in2=vcf2.out (pan 0.7): see "Choral flute".

## Signal flow (normalled: connected without cables)

KEYS (pitch, gate, velocity) → VCO 1-3 → MIXER → VCF 1 → VCF 2 → FORMANT → VCA → OUT → COMB → ENSEMBLE → DELAY →
REVERB → MAIN (what is heard). ENV 1 opens the VCA (loudness over time), ENV 2 moves the VCF 1 and VCF 2
cutoff, LFO 1 → the VCOs' FM inputs (vibrato, depth = each VCO's FM fader), LFO 2 → the VCA's Mod input
(tremolo, depth = VCA Tremolo). Every module is per voice up to the VCA; OUT and the effects are shared.

A cable replaces an input's normal connection; up to 4 cables per input add up, each with a level
(`input=output@0.5`). To keep the normal source and add another, cable both (e.g.
`vcf1.in=mix.out+vcf1.out@0.4` keeps the mixer and adds feedback). Effect sockets (comb, ensemble, delay, reverb,
out.out, main.in) connect only among themselves.

Modulation inputs (`vcoN.pitch`, `vcoN.fm`, `vcoN.pm`, `vcfN.cutoff`, `vcfN.res`, `formant.vowel`, `vca.cv`,
`vca.mod`, `lfoN.rate`) also take a negative level: the source inverted. `vcf1.cutoff=env2.out@-0.5` closes
the filter as the envelope rises (a "negative envelope amount"); `vca.mod=keys.pitch@-0.3` makes low notes
louder (keyboard scaling); with `vca.initial` 1, `vca.cv=env2.out@-1` is loud at rest and dips while the
envelope is up. Audio and gate inputs take 0 to 1 only. A level of 0 keeps a cable, silent.

## Modules and the parameters that matter

- **voice.count** 1-32 [8]: 1 for solo instruments (winds, brass, a lead), 6-16 for chords and pads.
- **VCO 1-3** (`vcoN.wave` Sine / Saw / Square / Triangle / Pulse; `vcoN.octave` -2..+2; `vcoN.tune` ±7
  semitones; `vcoN.fm` 0-12 st vibrato depth from LFO 1; `vcoN.pm` 0-10 phase-modulation index):
  Saw = bright, strings and brass; Square = hollow, clarinet; Triangle = soft, recorder; Sine = pure,
  flute and organ; Pulse = narrow and nasal, oboe, good under FORMANT. Detune VCO 2/3 by 0.05-0.15 st
  for width. PM: cable `vcoN.pm=vcoM.out` (an operator into another) for bells and electric pianos.
- **MIXER** (`mix.vco1..3`, `mix.noise` 0-1): VCO 1 at 1, the others 0 by default; noise 0.03-0.08 adds
  breath. **NOISE** `noise.colour` White / Pink / Brown; `noise.rand` output: random steps at
  `noise.rate` Hz (sample & hold), smoothed by `noise.smooth`.
- **VCF 1 / VCF 2** (`vcfN.cutoff` 20-18000 Hz, `vcfN.res` 0-1, `vcfN.env` ±5 octaves from ENV 2,
  `vcfN.key` 0-1 key tracking, `vcfN.slope` 36 dB / 24 dB / 12 dB / 6 dB, `vcfN.type` Lowpass / Highpass /
  Bandpass / Notch; `vcf2.routing` Serial / Parallel): the main tone control. Low cutoff = dark, high
  resonance = a whistling peak (at 1 it rings by itself). Cutoff 600-1500 Hz with env 1.5-3 octaves and
  a short ENV 2 = a plucked or brassy attack. VCF 2 stays open (18 kHz) unless used.
- **FORMANT** (`formant.vowel` 0-4 = A E I O U, gliding between; `formant.voice` Bass / Baritone / Tenor /
  Alto / Soprano / Child; `formant.res` 0-1; `formant.mix` 0-1, 0 = off): vowels. For voices: a Saw or
  Pulse source, mix 1, the voice type by range (Bass/Baritone male low, Tenor male high, Alto/Soprano
  female, Child), the vowel (A "aah", E "eh", I "ee", O "oh", U "ooo"), a little vibrato (LFO 1 at 5-6 Hz,
  FM 0.1-0.2 st). A cable `formant.vowel=lfo2.out@0.1` makes the vowel move.
- **VCA** (`vca.initial` 0-1 a level without any envelope; `vca.tremolo` 0-1 depth of LFO 2; `vca.velocity` 0-1
  how much the note velocity sets the loudness, 1 = soft notes quiet, 0 = every note full; the velocity comes
  in on `vca.vel`, normally `keys.vel`: a cable such as `vca.vel=noise.rand@0.5` gives random accents).
- **ENV 1 → VCA, ENV 2 → VCF** (`envN.attack` / `decay` 0-5 s, `sustain` 0-1, `release` 0-10 s):
  loudness and tone over time. Winds: attack 0.03-0.1, sustain 0.85-0.9, release 0.1-0.15. Strings:
  attack 0.15-0.4. Pads: attack 1-2, release 2-4. Plucked and struck (piano, guitar, marimba): attack
  0.001-0.002, decay 0.6-3, sustain 0. Organ: attack 0.005, sustain 1, release 0.02.
- **LFO 1 / LFO 2** (`lfoN.rate` 0.05-20 Hz, `lfoN.wave` Sine / Triangle / Saw / Square; Rate inputs):
  vibrato 5-6 Hz, slow movement 0.1-0.5 Hz. A cable `lfo1.rate=env2.out` makes the vibrato speed up.
  `lfoN.delay` 0-5 s (0 = at once): per note the LFO fades in after the start (nothing for half the
  delay, full at the delay) — delayed vibrato, 0.3-0.6 s for solo winds, strings and voices.
- **OUT** `out.gain` 0-1 (the default 0.8 ≈ -1.9 dB); a soft limiter catches peaks.
- **COMB** (`comb.freq` 20-5000 Hz, `comb.key` 0-1, `comb.feedback` -0.95..0.95, `comb.damping` 0-1,
  `comb.mix` 0-1, 0 = off): a resonant tube or body. Key 1 with Freq 261.6 (C4) follows the notes;
  positive feedback = hollow, all harmonics; negative = odd harmonics, clarinet-like.
- **ENSEMBLE** (`ensemble.depth` 0-1 how far the three chorus lines sweep, `ensemble.rate` 0.15-2.4 Hz the slow
  sweep (the fast shimmer is 10 x), `ensemble.shimmer` 0-1 the fast sweep's share, `ensemble.mix` 0-1, 0 = off): the
  string-machine (Solina) chorus between COMB and DELAY; module preset "Solina" (depth 0.7, mix 0.7) turns saw
  pads into a string ensemble; "Subtle chorus", "Wide", "Vibrato strings".
- **DELAY** (`delay.time` 0.001-2 s, `delay.feedback` 0-0.95, `delay.damping` 0-1, `delay.spread` 0-1
  where 1 = ping-pong, `delay.mix` 0-1, 0 = off): echoes; 0.03 s doubling, 0.09 s slapback, 0.3-0.5 s echo.
- **REVERB** (`reverb.mix` 0-1, 0 = off; `reverb.size`, `reverb.damping`, `reverb.width` 0-1): the room.
  Mix 0.1-0.15 small room, 0.25 hall (size 0.8), 0.35 cathedral (size 0.95).

## Recipes

- **Male "ooo"**: vco1.wave Pulse, vco1.octave -1, voice.count 1, formant.mix 1, formant.voice Baritone
  (or Bass), formant.vowel 4 (U "ooo"; 3 is O "oh"), formant.res 0.6, env1.attack 0.15, env1.release 0.4,
  lfo1.rate 5.2, vco1.fm 0.12, reverb Chamber. The preset "Male voice (ooo)" is this.
- **Female "aah" choir**: vco1.wave Pulse, VCO 2 Saw tuned 0.1 st mixed at 0.7, both FM 0.1, voice.count
  12, formant.voice Alto (or Soprano), vowel 0, res 0.6, mix 1, attack 0.35, release 0.9 (preset
  "Choir (aah)").
- **Breathy flute**: vco1.wave Sine, mix.noise 0.06 Pink, vcf1.cutoff 2500, attack 0.08, sustain 0.85,
  vibrato 5.5 Hz FM 0.15, COMB Key 1 Freq 261.6 feedback 0.4 mix 0.2, reverb Chamber (preset "Flute").
- **Brass**: Saw, VCF cutoff 900, env 2.5 octaves, ENV 2 attack 0.04 decay 0.3 sustain 0.6, voice.count 1.
- **Plucked string**: Saw or Pulse, cutoff 1200, env 2.5, ENV 1 and ENV 2 attack 0.002 decay 2 sustain 0,
  COMB Key 1 feedback 0.6 mix 0.3 (preset "Guitar (plucked)").
- **Bell / electric piano**: VCO 1 and 2 Sine, cable `vco1.pm=vco2.out`, vco1.pm 1-3, VCO 2 octave +1 or
  tune 7 st for a metallic ratio, decaying envelope (preset "Electric piano (FM)").
- **Pad**: two or three detuned Saws, cutoff 1200, res 0.3, attack 1.5, release 3, slow LFO, reverb 0.3.

## Working method

1. Read the current sound (`get_sound`) or start from the nearest preset.
2. Change a few parameters at a time (`set_parameters`), then `play` a note or a chord (e.g. 48 for a
   male voice, 60-67 for most, a chord [60, 64, 67] for pads) and describe what changed.
3. Keep effects modest; leave out.gain unless the sound is too quiet or loud.
4. When the user likes it, `name_sound` and offer `save_instrument_preset`.
   When they do not like a change: with lmw-mcp, `undo` takes it back (one step per parameter, cable change
   or preset); inside the synth's Assistant tab there is no undo tool, the musician has Undo last answer.
