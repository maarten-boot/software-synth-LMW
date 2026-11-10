# Conversation 2026-11-10

## Status and scratchpad clean-up
User asked for status: everything pushed, CI green (runs 42698563864, 42707223188). Phase 8b/8c still wait for permission to use third-party tools. Scratchpad cleaned from 946 MB to 231 MB (new global rule on temp files); kept the golden render, plan9port and AudioPluginHost.

## Downloadable builds (S9.3)
User: "do we have a windows build?" — CI built it but kept nothing. User: add downloads for all four platforms. Added Package + Upload build steps (actions/upload-artifact v7.0.2 pinned by SHA cf430e0) producing artifact LMW-Synth-<platform> with standalone, VST3, lmw9p, LICENSE, BUILD.txt; Unix builds tarred to keep executable bits; Fedora deps get tar/gzip. Spec S9.3, Tests T-S9.3 (planned), Plan row 8, README Download section.

CI run 42871851695 for 27427e1: green on all four; four artifacts (Windows 8.8 MB, Fedora 8.1 MB, Ubuntu 7.6 MB, macOS 5.4 MB), kept until 2027-02-08. Windows zip and macOS tarball contents checked. T-S9.3 pass.
