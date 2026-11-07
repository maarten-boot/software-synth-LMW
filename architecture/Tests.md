# Tests — software-synth-LMW

| ID | Spec | Test | Status |
|---|---|---|---|
| T-S2.1 | S2.1 | Project builds and starts on Linux, macOS, Windows | Build + ctest OK on Linux, macOS, Windows (CI run 42385798382, 2026-11-07); "starts" not yet verified (no display) |
| T-S1.2a | S1.2 | With no cables, default patch produces sound on a MIDI note | planned |
| T-S1.2b | S1.2 | A cable into an input replaces (not adds to) its normalled source | planned |
| T-S3.2 | S3.2 | Computer keyboard keys trigger notes in standalone | implemented (JUCE MidiKeyboardComponent); manual check pending |
| T-S5.1-osc | S5.1 | Each waveform has correct pitch (440 Hz ±1) and bounded output | pass (`test_engine`) |
| T-S5.1-adsr | S5.1 | ADSR attack/decay/release durations exact to ±2 samples; zero times safe | pass (`test_engine`) |
| T-S5.1-engine | S5.1 | Silence before note, sound during, silence after release; only matching note-off releases | pass (`test_engine`) |
| T-S6.2b | S6.2 | All theme colour pairs (src/ui/Theme.h) >= 4.5:1 | pass (`test_contrast`) |
| T-S4.1 | S4.1 | Setting polyphony to N allows exactly N simultaneous notes (N=1,8,32); range clamps to 1..32 | pass (`test_engine`) |
| T-S4.2 | S4.2 | Playing N+1 notes steals the oldest voice; released voices stolen first | pass (`test_engine`) |
| T-S4.3 | S4.3 | Same note twice uses one voice | pass (`test_engine`) |
| T-S4.4 | S4.4 | Steal causes no sample jump > 0.02 | pass (`test_engine`) |
| T-S4.5 | S4.5 | Lowering voice count 8→2 leaves 2 active after release | pass (`test_engine`) |
| T-S6.2 | S6.2 | Every UI colour pair computes >= 4.5:1 contrast | unit test `test_contrast` passes; UI-wide check pending |
| T-S6.3 | S6.3 | Each preset/custom ratio from config and UI is enforced | enforcer unit-tested; config/UI pending |
| T-S7.1 | S7.1 | Save then load a patch restores all parameters and cables exactly | planned |
| T-S8.2 | S8.2 | Every dependency has an AGPLv3-compatible licence (list in repo) | planned |
| T-S9.2 | S9.2 | VST3 loads, plays and saves state in at least 2 hosts (e.g. Reaper, Bitwig) | planned |
| T-S10.2 | S10.2 | Under 9P load (1000 writes/s) no audio dropouts; RT thread does no syscalls (checked with RT sanitizer/trace) | planned |
| T-S10.1 | S10.1 | 9P2000 conformance: version/attach/walk/open/read/write/clunk/stat against the plan9port `9p` client | planned |
| T-S10.1b | S10.1 | `echo 1000 > params/vcf/cutoff` via 9P changes the audible/internal value | planned |
| T-S10.4 | S10.4 | Engine reachable over Unix socket on Linux/macOS/Windows and over TCP when enabled | planned |
| T-S10.5 | S10.5 | Two plugin instances in one host expose two distinct 9P endpoints | planned |
| T-S10.9 | S10.9 | `cat source` returns the repository URL | planned |
