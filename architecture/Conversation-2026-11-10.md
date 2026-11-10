# Conversation 2026-11-10

## Status and scratchpad clean-up
User asked for status: everything pushed, CI green (runs 42698563864, 42707223188). Phase 8b/8c still wait for permission to use third-party tools. Scratchpad cleaned from 946 MB to 231 MB (new global rule on temp files); kept the golden render, plan9port and AudioPluginHost.

## Downloadable builds (S9.3)
User: "do we have a windows build?" — CI built it but kept nothing. User: add downloads for all four platforms. Added Package + Upload build steps (actions/upload-artifact v7.0.2 pinned by SHA cf430e0) producing artifact LMW-Synth-<platform> with standalone, VST3, lmw9p, LICENSE, BUILD.txt; Unix builds tarred to keep executable bits; Fedora deps get tar/gzip. Spec S9.3, Tests T-S9.3 (planned), Plan row 8, README Download section.

CI run 42871851695 for 27427e1: green on all four; four artifacts (Windows 8.8 MB, Fedora 8.1 MB, Ubuntu 7.6 MB, macOS 5.4 MB), kept until 2027-02-08. Windows zip and macOS tarball contents checked. T-S9.3 pass.

## Phase 8b: validators built from source
User: "build the two validators from source; tell me what to install, you have no root access". Built Steinberg validator (vst3sdk v3.8.1_build_84, only the validator target; a placeholder gtkmm-3.0.pc lets the SDK configure, its editorhost needs gtkmm) and pluginval v1.0.4 (LADSPA hosting off, ladspa.h not installed). Nothing needed installing.
- Steinberg validator: 46/47 — "Programlist 000->Program 000: has no name". Fix: getProgramName returns "Default". Then 47/47.
- pluginval strictness 10: SUCCESS headless (21 groups) and on Xvfb :95 with editor tests (25 groups).
- scripts/validate-vst3.sh: clones both at pinned commits (checked), builds, runs; work dir /tmp/<project>/validators. Spec S9.4, Tests T-S9.4.

## Validators in CI
User: add the validators to CI. scripts/validate-vst3.sh made portable (Linux/macOS/Windows Git Bash: default CMake generator, --config Release, .exe / pluginval.app, placeholder gtk+-3.0/gtkmm-3.0 .pc on Linux only, LADSPA switch Linux only) and copies the built programs to <work>/bin, which is all CI caches (actions/cache v6.1.0, key: platform + script hash). Fresh build 3m20s locally, cached run ~1 s. New CI steps "Validator cache" and "VST3 validators" after Test.

CI run 42885837808 for dd0b256: Ubuntu (47/47, pluginval 21 groups) and macOS (47/47, pluginval 25 groups incl. editor) pass. Windows: pluginval v1.0.4 fails to compile with MSVC (100ms literal, <chrono> not included) -> /FIchrono via CXXFLAGS on Windows. Fedora: pluginval links -static-libstdc++ -> libstdc++-static added to the Fedora deps.
