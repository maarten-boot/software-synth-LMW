# Conversation 2026-11-17

**User (pasted request from the Kukel project, docs/future.md §13):** changes to the 9P server for the Kukel GUI, in order: (1) read-only /info, one JSON array describing every parameter (id, name, kind, default; min/max; float unit, centre, step; choices), from createLayout, same output as ../kukel/examples/lmw/fakesynth.kkl _params(); (2) exact float round trip (800 read back as 800.0001); (3) remove the socket file on SIGTERM; (4) rebuild build/ (binary of 2026-11-12, before f733754, refused [schematic]); (5) later, optional: /serial and /keys. Keep existing files, formats, error texts and 9P2000.
Done 1-4; 5 left for later (asked as optional):
- /info: createLayout records per float its unit, default and skew centre (FloatInfo); parameterInfo() writes the JSON from the real parameters (min/max/step from the ranges), one object per line, floats with a decimal point.
- Exact floats: valueText gives the shortest text (1-9 digits, no exponent) inside the range that maps to the same stored float. First try printed "8e+02" (%g) and "20000" for 18000 (a clipped candidate) — both fixed; host test now writes 375 values over all float parameters, all exact.
- Signals: listeners register their socket path in fixed slots; the standalone's handler for SIGTERM/SIGINT/SIGHUP unlinks them (async-signal-safe) and a 100 ms timer quits normally (state saved: cutoff 777 survived a SIGTERM restart); a second signal kills.
- build/ rebuilt natively (20/20 ctest); a patch with [schematic] loads.
- Kukel checks: ../kukel/bin/kukel (2026-10-16) cannot parse today's lmwctl.kkl (lmwctl.kkl:110:5: expected expression, found '}'); built a current kukel from ../kukel source into the scratchpad (Kukel repo untouched). Then `lmwctl params` equals `-fake params` (51 lines), `info vcf1.cutoff` as expected, `set vcf1.cutoff 800 , get vcf1.cutoff` prints 800.0 — the same as the fake does.

**User:** pushed, wait for CI
CI: run 44173477240 for 07e4331: Ubuntu 24.04, Fedora 45, Windows green; macOS red — host test 372 of 375 exact: a 0 written to vco1-3.tune read back "-2e-07" (JUCE snaps to the 0.01 step as -7 + 0.01*700, which is not exactly 0 in macOS float arithmetic; only significant-digit roundings of the stored value were tried). Fix: shortestNumber also tries rounding to 0-9 decimals and keeps the shortest accepted text ("0"). Linux 375/375, 20/20 ctest; build/ rebuilt.

**User:** pushed, wait for CI
CI: run 44179881925 for c1dbb5d: all four platforms green; host test passes on macOS too (14.6 s), so /info and the exact float round trip pass everywhere.

**User:** pushed, wait for CI
CI: run 44179881925 for c1dbb5d: all four platforms green; host test passes on macOS too (14.6 s), so /info and the exact float round trip pass everywhere.
