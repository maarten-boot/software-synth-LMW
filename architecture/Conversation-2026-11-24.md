# Conversation 2026-11-24

- User pushed and tagged v1.4.0 (2026-11-23). Build run 45198363354 (40efde0) green on all four. Release run 45199716349: the macOS job was not given a runner by GitHub ("not acquired by Runner ... after multiple attempts", macOS arm64 capacity), cancelled, publish skipped.
- User re-ran the failed job: macOS, publish green. Release 1.4.0 published 2026-11-24 with all six files.

## Visual enhancements (Phase 24)

- User: box around the selected in/out socket, golden yellow, colour in the colour settings; normal view always fills the entire screen, units keep their size (moving space, room for new units); compact the same with compact panels; feedback on the faders whether Linear / Log / Other.
- Typos confirmed in the answer: anhancements, allw, colr, nowmal.
- Asked: golden yellow on white is 1.6:1 → yellow fill + black edge; full screen → resizable, opens full screen (VST3 resizable at today's size); compact → compact panels, still full screen; fader feedback → ticks, tag and hover line.
- Spec S6.15 highlight, S6.16 window, S6.17 fader scale drafted; Plan 24a–24c; tests T-S6.15/16/17 planned. To confirm.
- User: all ok; commit the spec and start 24a; if it helps add a horizontal line between the racks (rack type layout) → added to S6.16 as rack rails (part of 24b).
- 24a done: theme item highlight (ui/Theme.h: Colours/Palette.highlight, item 11, presets, raised to the contrast; Config comment), Jack::paint box (focus, hover, drop target; hover repaints), Schematic ports (hover, keyboard cursor, drop target; replaces the cursor square and drop ring). Unit tests in test_contrast. Xvfb: hover, drag (source stays lit too: noted in S6.15), Schematic, Edit colours row, Max, custom green. 35 of 35.
- User: commit 24a (6506578) and start 24b.
- 24b done: editor resizable (edges in the standalone, corner in a DAW), minimum from the rack (setResizeLimits), grows only in the standalone (Compact / keyboard keep the size), a plugin fits the view on those toggles; RackLayout::setMaxColumns: rows as wide as the widest screen; StandaloneApp fillScreen: work area of the window's screen at every start; rack rails (2 px, cable grey) on Synth and Effects. Found: JUCE's standalone window takes its limits from the editor (decorator constrainer: a window setResizeLimits would be ignored), and on Linux the WM minimum is only sent with new bounds → one-pixel resize when the minimum changes. Keyboard: 88 keys get wider (no more keys possible): noted in S6.16. Xvfb checks and host test (resizable). 35 of 35.
- User: keep the keys at a normal size, the free space later for a mod wheel or glissando / portamento (typos confirmed: kees, glisando); commit and start 24c. Keys at the default-layout width (28 px, compact 21), keyboard at the right, free space left (Plan L11). Xvfb checked.
- User: commit 24b (fa51f07), start 24c.
- 24c done: src/ui/FaderScale.h (kinds, ticks, labels, hover words; JUCE-free), tests/test_scale.cpp (T-S6.17-scale); processor scaleOf() and /info "scale"; ScaleSlider ticks in LMWLookAndFeel::drawLinearSlider (ticks left, values right in normal view), a tag label under each name (kept free on every fader), hover line replaced (was "Scale: skewed, half way = ..."). Found on Xvfb: the Effects faders' tags showed on the Synth tab (not in the tab list) → fixed. Host test /info expectations updated. 36 of 36.

## Crosscheck after Phase 24

- User: commit 24c (d087ebe) and do the crosscheck.
- Spec items written before Phase 24 that it changed, marked with "since Phase 24x": S3.2 keyboard keys (normal width at the right), S6.1 hover scale words (S6.17), S6.4 sizes now the minimum, S6.5 row width / window size, S6.6 and S6.7 drop ring and port cursor (highlight box), S6.12 hidden keyboard (standalone keeps its size), S6.13 theme items and color_ keys (highlight); S6.16 sizes as measured.
- Code vs Spec S6.15–S6.17: all as built (Synth, Effects and Schematic sockets; Matrix has no sockets; tags and ticks on Synth and Effects; /info scale). Plan rows 24, 24a–c, L11 match. Tests T-S6.15/16/17 pass.
- Open: the tutorial screenshots (synth.png, matrix.png, ...) show the 1.4.0 look (no tags, ticks, rails); not regenerated (done below).
- User: commit (0baba57) and regenerate the screenshots. docs/images/synth, effects, matrix, schematic.png taken again on Xvfb (1496 × 904 window, the String section preset, as before): ticks, tags, rack rails, the Menu header, 1.4.0; effects cropped to the panels. T-S9.8-html passes.
- User pushed (407d8ba). CI run 45395252588 green on all four.

## Cable faders and inverted modulation (Phase 25)

- User: horizontal faders for every connection shown in a socket's right-click menu, output name left, input name right, stacked at the bottom of the menu, on Synth, Effects and Schematic; inverted inputs for modulation (inverted ADSR on cutoff, resonance, FM, PM), an invert button or 0 in the middle; audio inputs do not need it; CV to scale loudness / modulation over the keyboard.
- Typos confirmed in the answer: holrizontal, ths, schema view (= Schematic tab).
- Advised: bipolar level on modulation inputs, menu custom items, a drag one undo step; noted that −env on VCA CV gives silence (no offset).
- Asked: bipolar level 0 in the middle; faders for cables and normalled connections; plain negative everywhere (no offset); up to 100 %.
- Spec S5.21 (bipolar levels) and S6.18 (menu faders) drafted; Plan 25, 25a–c; tests T-S5.21-bipolar, T-S6.18-menu planned. To confirm.
- User: all ok, commit the spec and start 25a. Note for the future: resonators for drum and percussion effects and presets with "native instruments"; current native instruments to add (samisen, Japanese flute, didgeridoo, gamelan, ...). Typo "foe" (for); "native instruments" read as traditional / ethnic instruments, not the company → Plan L12, L13.
