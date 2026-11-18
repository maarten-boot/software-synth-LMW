# Conversation 2026-11-18

**User:** status ? — answered: all five Kukel items done; 6f813f5 (/serial, /keys) not yet pushed.

**User:** pushed, wait for CI
CI: run 44287223002 for 6f813f5: all four platforms green (host test with /serial and /keys checks passes on macOS 14.8 s, Windows 7.9 s, Fedora and Ubuntu 7.9 s). Only warning in the logs: the older -Wfloat-equal on the tail-length check (HostTest.cpp:297).

**User:** commit the docs; can i inform the Kukel project we now have the /info on the 9p ?
Committed; yes — gave a message for the Kukel project (all five items, formats, how to check).
