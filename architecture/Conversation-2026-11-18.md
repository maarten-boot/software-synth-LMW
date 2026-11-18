# Conversation 2026-11-18

**User:** status ? — answered: all five Kukel items done; 6f813f5 (/serial, /keys) not yet pushed.

**User:** pushed, wait for CI
CI: run 44287223002 for 6f813f5: all four platforms green (host test with /serial and /keys checks passes on macOS 14.8 s, Windows 7.9 s, Fedora and Ubuntu 7.9 s). Only warning in the logs: the older -Wfloat-equal on the tail-length check (HostTest.cpp:297).

**User:** commit the docs; can i inform the Kukel project we now have the /info on the 9p ?
Committed; yes — gave a message for the Kukel project (all five items, formats, how to check).

**User:** status ? — all pushed and green; nothing pending.

**User:** (from the Kukel session) floats do not read back exactly: 1234.5678 → 1234.5673, 20.000001 → 20.000002; fits the normalised 0–1 fader position kept as a 32-bit float; the 375 test values had no long numbers; Kukel's fake keeps the value as given (docs/future.md §13.8).
Confirmed: 1234.5673 and 1234.5678 store the same value after the 0–1 round trip (20.000001 is already 20.000002 as a 32-bit float). Stored precision cannot be raised through JUCE's float parameters, so the synth now remembers the text last written per float parameter (9P write, patch load) and gives it back while it still stores the current value; otherwise the shortest text as before. /patch uses it too. Host test 4d: long values added (426 of 426 exact), /patch, clip and patch-load checks; read9P now runs the message loop (reading /patch needs it). Spec S7.1/S10.10, Tests T-S10.10-exact2, Changes updated. Native build and Ubuntu 24 container: 20 of 20 pass.

**User:** A) version not shown anywhere: in an About menu or after the name in the top bar. B) named presets per component (ADSR, VCF, VCO, reverb), after classical instruments (flute, recorder, oboe, clarinet) as starting points; Matrix: a slider on the right, active for each position that has 0–100 %.
Suggested corrections: "menue" → "menu", "favouritest" → "favourites", "en" → "and", "recorde" → "recorder", "hoboe" → "oboe". Asked: version → Both (title + About); modules → ENV, VCF, VCO, Reverb; own presets → built-in + save own; picker → menu in the title bar.
Done (Phase 13): S6.10 version (title button opening About, About... button, /version, commit from git at every build); S6.8 level slider for the cursor cell (off on gates and full inputs); S7.5 module presets (ModulePresets.h, 53 built-in, .lmwpreset files). Xvfb check found and fixed: name fields of the Save and type-level dialogs not focused; slider took no focus (cursor frame vanished); compact header overflow (About covered the Notes tab) → title is the About button there, "Reset". Tests: test_presets, host test 4f (246 preset values over 9P), /version. Native and Ubuntu 24 container: 21 of 21 pass. Not done: presets over 9P (Kukel could use them later).

**User:** pushed, wait for CI — run 44348905763 for 46bceed (watching).

**User:** no installer like mini-daw's; can we get a Windows installer so installing and updating becomes easier? (correction: "can ge get" → "can we get")
Asked: VST3 place → ask at install (Just me / All users); version scheme → 0.<phase>.<round> (then replaced, see below).
**User:** where did 0.1.0 come from? After the installer move to v0.2.0 and continue until a sufficient product for version 1 (mostly cosmetic UI things now). (corrections: "whwre" → "where", "untill" → "until")
Answer: 0.1.0 was set in CMakeLists.txt with the Phase 0 skeleton and never raised. Now 0.2.0, raised per release.
Done (Phase 13b, S9.5): installer/LMW-Synth.nsi after mini-daw's (NSIS 3.13 pinned from Chocolatey in CI), mode page with UAC restart for all users, refusals (other mode, downgrade), in-use checks, uninstaller with elevation for all users; CMake target `installer`; CI builds it, runs scripts/test-installer.ps1 and adds the setup to the Windows download; README, THIRD_PARTY.md (VST3 SDK: MIT). Not testable here (no makensis without your permission): compiled and tested only in CI.

CI: run 44348905763 for 46bceed (Phase 13): all four green (host test 12.9 s Linux, 35.1 s macOS, 15.4 s Windows; T-S7.5-presets; validators 47 of 47).

**User:** pushed, wait for CI — run 44352426051 for b191ed5 (installer, 0.2.0): Windows green, the installer test 32 of 32 in both modes (makensis v3.13); Ubuntu green; Fedora failed in the VST3 validators: pluginval reported SUCCESS and then crashed on exit (segmentation fault) — new, no plugin code changed since the green run; macOS cancelled before it started (no runner within 15 minutes; no steps ran).

**User:** the synth is sufficiently mature to move to v1.0.0 and a release CI flow; add a monitor to clean up artifacts and releases older than 14 days after we have more than 2 releases. (corrections: "synt" → "synth", "then" → "than" twice)
Asked: cleanup → keep the 2 newest releases, others older than 14 days deleted, artifacts older than 14 days only once there are more than 2 releases; tags kept; release by pushing tag v1.0.0; monitor daily.
Done (Phase 14, S9.6, S9.7): version 1.0.0; release.yml (tag = version check, build.yml as reusable workflow, publish with gh release create); build.yml no longer runs for tags; cleanup.yml + scripts/cleanup-select.sh + tests/test_cleanup.sh (ctest T-S9.7-cleanup); Changes.md 1.0.0 section with a summary; README. Native: 22 of 22 pass.
