# Tests — software-synth-LMW

| ID | Spec | Test | Status |
|---|---|---|---|
| T-S2.1 | S2.1 | Project builds and starts on Linux, macOS, Windows | planned |
| T-S1.2a | S1.2 | With no cables, default patch produces sound on a MIDI note | planned |
| T-S1.2b | S1.2 | A cable into an input replaces (not adds to) its normalled source | planned |
| T-S3.2 | S3.2 | Computer keyboard keys trigger notes in standalone | planned |
| T-S4.1 | S4.1 | Setting polyphony to N allows exactly N simultaneous notes (N=1,8,32) | planned |
| T-S4.2 | S4.2 | Playing N+1 notes steals the oldest voice, no crash/click burst | planned |
| T-S6.2 | S6.2 | Every UI colour pair computes >= 4.5:1 contrast | planned |
| T-S6.3 | S6.3 | Each preset/custom ratio from config and UI is enforced | planned |
| T-S7.1 | S7.1 | Save then load a patch restores all parameters and cables exactly | planned |
| T-S8.2 | S8.2 | Every dependency has an AGPLv3-compatible licence (list in repo) | planned |
| T-S9.2 | S9.2 | VST3 loads, plays and saves state in at least 2 hosts (e.g. Reaper, Bitwig) | planned |
| T-S10.2 | S10.2 | Under 9P load (1000 writes/s) no audio dropouts; RT thread does no syscalls (checked with RT sanitizer/trace) | planned |
| T-S10.1 | S10.1 | 9P2000 conformance: version/attach/walk/open/read/write/clunk/stat against the plan9port `9p` client | planned |
| T-S10.1b | S10.1 | `echo 1000 > params/vcf/cutoff` via 9P changes the audible/internal value | planned |
| T-S10.4 | S10.4 | Engine reachable over Unix socket on Linux/macOS/Windows and over TCP when enabled | planned |
| T-S10.5 | S10.5 | Two plugin instances in one host expose two distinct 9P endpoints | planned |
| T-S10.9 | S10.9 | `cat source` returns the repository URL | planned |
