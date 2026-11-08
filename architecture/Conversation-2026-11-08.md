# Conversation 2026-11-08 — software-synth-LMW

(Continues Conversation-2026-11-07.md; the session ran past midnight.)

## 1. Phase 3 completed
Processor: all 19 parameters (Spec S5a.1) with unit text (new: Hz/kHz, octaves, semitones + tests), mod wheel CC1 → engine, PatchCommand SPSC queue (requestPatch) for cables. GUI rewritten data-driven: row 1 POLY | VCO | MIXER | VCF | LFO | OUT, row 2 ENV 1 → VCA | ENV 2 → VCF; window 954×640. A combined build+screenshot command was killed (exit 137) mid-way; rerun separately: build OK, ctest 5/5, screenshot OK with all defaults correct.

## 2. Question (user)
> status?

## 3. Push + CI (user)
> pushed, wait for CI

CI run 42474471773 (9aa93cd): ubuntu-24.04 and fedora-45 pass (5 suites); macOS and Windows failed at configure — GitHub returned 504 for the JUCE archive download (503 from here too at that moment). Not a code problem.

## 4. Request (user)
> yes, add the JUCE cache

JUCE pin moved to cmake/JuceVersion.cmake; cmake/FetchJuceArchive.cmake downloads with 6 retries (10–50 s backoff) and verifies SHA-256 (skips if a verified copy exists); CMake option LMW_JUCE_ARCHIVE uses a local copy (hash still checked by FetchContent). Workflow: actions/cache v6.1.0 (latest, pinned 55cc8345…) on .juce-cache keyed by hashFiles(cmake/JuceVersion.cmake), then fetch, then configure with the archive. Verified locally (cached / corrupt copy, configure from local file). JUCE 9.0.5 still latest. Tests T-S2.3 updated, T-S2.3b added.
