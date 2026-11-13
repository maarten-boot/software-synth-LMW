# Conversation 2026-11-13

CI run 43310233189 for 13ca504 (unit colours): all four platforms green.

## Phase 12a: Schematic tab
User: "ENV as ADSR curve ok, start Phase 12; OUT symbol: a circle with a arrow leaving the circle , pointing to the right (as normally used oun sound cards); KEYS: a small keyboard with 4 keys and one black key as a start to see how it fits. Noise : example at: https://en.wikipedia.org/wiki/White_noise#/media/File:White_noise.svg"
(Read as: "used on sound cards"; the noise example is the usual time plot of white noise — drawn as a fixed pseudo-random trace.)
Done (view only):
- src/ui/Schematic.h (pure C++): blocks, port positions, connections from the patch, Bezier curves, hover texts, hit test; tests/test_schematic.cpp (T-S6.7), 20/20 ctests.
- src/app/SchematicView.h/.cpp: draws the blocks in their unit colours, normalled lines dashed behind the blocks, cables solid with a white edge on top (levels below 100 % in a box), names last on a white backing; tab Synth | Effects | Matrix | Schematic | Notes.
- Iterations on Xvfb: ENV 1 next to ENV 2 looked like ENV 1 → ENV 2 -> stacked them; a keys.vel cable vanished behind MIX at in3 (looked like a mixer input) -> cables on top; cables crossed block names -> names last on white.
- Checked live with lmw9p (cables with levels appear at once), hover text, compact view.
Open: 12b editing in the schematic — scope for the user.

User: "would the schematic not need also the keyboard tracking or is that only shown when not 0?" -> it was not shown at all (a fader, not a patch connection); proposed a third line style "internal (set by a fader)": keys.pitch -> VCF N while Key > 0 with "key N %", keys.vel -> VCA always, mod wheel only in the VCO hover text.
User: "yes, add it that way" -> done: Schematic.h internalLinks/internalTarget/internalCurve/blockNote, view draws dotted arrows into the block from below with labels, polls the Key faders (repaint on change); test_schematic extended (T-S6.7c); checked on Xvfb with lmw9p.
