# Conversation 2026-11-20

**User:** inspect the current directory and report
Reported: clean tree on main at b319344 (v1.1.0 released); Phases 0–6 and 8–15a done, Phase 7 (Kukel GUI) pending; 2 releases (1.1.0, 1.0.0), oldest CI artifact 2026-11-10, so no cleanup warning (needs > 2 releases and artifacts > 14 days); build/ 429 MB and build-u24/ 18 MB ignored. Stale: Plan Phase 13 "CI pending", last crosscheck 2026-11-14, Handover.md a 2026-11-07 snapshot.

**User:** yes, do the crosscheck and fix the stale lines
Crosscheck Spec ↔ Plan ↔ code for Phases 13–15a and release 1.1.0. Code matched the Spec: 53 built-in presets (ModulePresets.h), preset_/presetValues_ state properties, label refresh every 8th tick of 30 Hz (about 4/s), format = 1 checks, /version over 9P, About dialog, release.yml tag check and Changes.md section, cleanup.yml (cron 17 3 * * *, KEEP 2, 14 days, dry run default by hand), build.yml paths-ignore architecture/** and workflow_call, NSIS 3.13.0 check, artifacts expire after 90 days, 22 ctest entries.
Fixed:
- Plan: crosscheck line; Phase 13 "CI pending" → green (run 44348905763); Phase 14 notes v1.1.0 (run 44398812297); Phase 15a CI run 44389641194, released in 1.1.0.
- Tests: T-S2.1b "20 now" → 22 (21 on Windows); CI run 44348905763 added to T-S6.10-version, T-S7.5-presets, T-S7.5-values.
- Spec S7.5: file-name rule completed from the code (non-ASCII kept, spaces trimmed, empty → "Preset").
- Spec S9.7: scheduled 03:17 UTC, but GitHub started the runs at about 11:52 UTC on 11-19 and 11-20.
- Handover.md: current-status line, sections 3–5 marked out of date.
Still open (not stale): S4.2 voice-stealing preference (release before held) awaits the user's confirmation.
