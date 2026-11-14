# Conversation 2026-11-14

CI: run 43445485584 for f733754 (Phase 12c) and run 43522332391 for 1ccf7d9 (names inside symbols, keyboard octaves): all four platforms green.

User: "the synth is already tested in my mini-daw ( see ../mini-daw) but feel free to test it with an other DAW" (correction offered: "another DAW"). Taken as permission for Phase 8c with a DAW built from source: Ardour 9.9 (tag commit 9500a47c), built in an Ubuntu 24.04 container, headless test with its Lua session tool. mini-daw's own test (T-VST-40, 2026-11-10) noted in the Plan.

User: "the color for the mixer can be a very very light gray" — MIXER gets #F0F0F0 (S6.9): title bar, matrix labels (mix.*), schematic MIX symbol.

CI: run 43591478569 for 888c9cb (MIXER grey): all four platforms green.

Phase 8c, Ardour 9.9 (built from source, tag commit 9500a47c, in an Ubuntu 24.04 container image with its build dependencies; first build about an hour):
- Snags on the way, none in the plugin: a read-only CA-bundle mount broke the ca-certificates package (mount it elsewhere, GIT_SSL_CAINFO); Ardour 9.9 also needs libjpeg-dev; waf needs python3; Ardour only trusts a VST3 cache file strictly newer than the module (copy, wait a second, scan); in Lua the route group must be ARDOUR.RouteGroup() (nil segfaults, also with Ardour's own synth); a track named "Synth" clashes with a port; the track meter's channel 0 is MIDI; closing a second session hung while the script still held references to the first plugin.
- Headless (scripts/test-ardour.sh, new): scan, load, note C4 −18.3 dBFS, silence before/after, Volume and Cutoff from Ardour reach the plugin, session save/reload restores them, plays after reload: 15/15 PASS.
- Found: headless, 9P writes answer "busy". JUCE's VST3 wrapper takes the host's IRunLoop as message thread; Ardour runs it on the GLib main loop, which only the GUI iterates. Expected (S10.12), noted in the Spec.
- GUI on Xvfb (Dummy backend shown via hide-dummy-backend = 0): 9P writes in ~4 ms, editor opens from the mixer strip with the 9P changes visible, save + quit with the editor open, restart: cutoff, cable @0.25 and notes restored.
