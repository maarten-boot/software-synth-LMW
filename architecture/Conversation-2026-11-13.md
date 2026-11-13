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

CI: run 43420939527 for 663c5d2 (Phase 12a) and run 43422487432 for 831ee81 (internal links): all four platforms green.

## Phase 12b: editing in the schematic
User: "yes, add editing in the schematic like the Synth tab".
Done: ui/CableEdit.h holds the drag rules (grab / drop -> list of connect ops, applied in order, stopping at the first failure); the Synth tab's cableDrag/cableDrop now use it too, so both tabs behave the same. showJackMenu became showPortMenu(output, port, screenArea), used by both. SchematicView: mouse drag with rubber cable and target ring, plugged inputs with a black centre, arrow-key port cursor, Enter / Shift+F10 / right-click menu, Delete, Escape. test_schematic extended (T-S6.7d), 20/20; Xvfb checks (T-S6.7e) incl. the Synth tab after the refactor. Phase 12 done.

## Crosscheck after Phase 12
User: "do the crosscheck first". Checked Spec S6.6–S6.9, S7.2/S7.4, S9.2 against the code (drag rules shared by both tabs, menus, keyboard, focus on tab open, contrast pairs incl. unit colours, 51 parameters, 20 ctests). Code matched; doc fixes only: S7.4 still listed the tabs as Synth | Effects | Notes, Plan Phase 12 still said "open for the user", T-S2.1b test count 19 -> 20. Handover.md is a dated snapshot and left as is.

CI: run 43441390442 for 49394f8 (Phase 12b) and run 43441968495 for 61ea6eb (crosscheck): all four platforms green.

## Phase 12c: movable symbols
User: "pushed; could we make the symbols movable in the schema view ?" ("schema view" = the Schematic tab).
Asked: saved where -> "State and patch files"; overlap -> not allowed; keyboard/reset -> Shift+arrows, Reset button.
Done: Schematic.h Layout (positions, fits/move with grid and no overlap, serialize/parse by block id; all geometry functions take a layout); PatchFile [schematic] section (only when moved; unknown sections now skipped); processor property schematicLayout, written to / read from patch files; SchematicView drags symbols by the body with a landing outline (dashed = refused), Shift+arrows, hand/crosshair pointer; editor: Reset layout per tab, "Reset schematic layout" in the port menu, layout picked up after a patch load. Tests T-S6.7f (unit) and T-S6.7g (Xvfb), 20/20. Removed the half-unit margin idea: the default drawing has symbols half a unit apart, so touching is allowed.
