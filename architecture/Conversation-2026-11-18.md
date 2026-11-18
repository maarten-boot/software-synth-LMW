# Conversation 2026-11-18

**User:** status ? — answered: all five Kukel items done; 6f813f5 (/serial, /keys) not yet pushed.

**User:** pushed, wait for CI
CI: run 44287223002 for 6f813f5: all four platforms green (host test with /serial and /keys checks passes on macOS 14.8 s, Windows 7.9 s, Fedora and Ubuntu 7.9 s). Only warning in the logs: the older -Wfloat-equal on the tail-length check (HostTest.cpp:297).

**User:** commit the docs; can i inform the Kukel project we now have the /info on the 9p ?
Committed; yes — gave a message for the Kukel project (all five items, formats, how to check).

**User:** status ? — all pushed and green; nothing pending.

**User:** (from the Kukel session) floats do not read back exactly: 1234.5678 → 1234.5673, 20.000001 → 20.000002; fits the normalised 0–1 fader position kept as a 32-bit float; the 375 test values had no long numbers; Kukel's fake keeps the value as given (docs/future.md §13.8).
Confirmed: 1234.5673 and 1234.5678 store the same value after the 0–1 round trip (20.000001 is already 20.000002 as a 32-bit float). Stored precision cannot be raised through JUCE's float parameters, so the synth now remembers the text last written per float parameter (9P write, patch load) and gives it back while it still stores the current value; otherwise the shortest text as before. /patch uses it too. Host test 4d: long values added (426 of 426 exact), /patch, clip and patch-load checks; read9P now runs the message loop (reading /patch needs it). Spec S7.1/S10.10, Tests T-S10.10-exact2, Changes updated. Native build and Ubuntu 24 container: 20 of 20 pass.
