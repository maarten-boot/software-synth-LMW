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
