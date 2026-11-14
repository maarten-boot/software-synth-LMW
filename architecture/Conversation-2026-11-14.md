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

CI: run 43604676546 for 28244a3 (Phase 8c, Ardour test): all four platforms green.

**User:** do the crosscheck first
Crosscheck after Phase 8c and the MIXER grey: code matches the Spec (Theme.h tints = S6.9 hex values, unitOf "mix", 5 s "busy" timeout S10.12, test-ardour 48 kHz). Docs fixed: Plan 11b still called MIXER white; Plan 12 status began "in progress" though done; T-S9.2 status "partly" → "pass on Linux". Handover.md is a dated snapshot, left as is.

**User:** pushed, wait for CI
CI: run 43620626820 for 6c3ae4d (crosscheck after Phase 8c): all four platforms green.

**User:** add the T-S10.12 busy test
Added as section 4b of the VST3 host test (T-S9.2-host): the host's main thread is the plugin's message thread, so without a running message loop a 9P write to params/vcf1/cutoff answers "busy" — Linux: after 5.08 s, cutoff unchanged; after runDispatchLoopUntil the late change is applied (1234 Hz); with the loop running a write takes 1 ms. Host test 5.7 s now (was under 1 s). 20/20 ctests pass.

**User:** pushed, wait for CI
CI: run 43624788268 for b4dce95: Ubuntu 24.04, Fedora 45, Windows green (host test 5.7-5.9 s); macOS red — T-S10.12 found a real bug: "busy" came after 14.53 s, since NinePService counted 50 ms waits instead of measuring time (macOS timer coalescing makes each wait longer). Fixed with a steady_clock deadline; Linux now 5.01 s, 20/20 pass.

**User:** pushed, wait for CI
CI: run 43627686113 for 41a7655 (busy-timeout fix): all four platforms green; host test 5.65-5.86 s everywhere, macOS included. T-S10.12 marked pass.
