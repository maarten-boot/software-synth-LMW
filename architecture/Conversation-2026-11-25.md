# Conversation 2026-11-25

- Session continued (context compacted). User: continue → read as yes to "commit, then 27b".
- Committed 27a and the computer-key fix.
- 27b done: src/app/TermsView.{h,cpp} (Find, "N of M terms", Show all, list by topic, See also buttons), Menu / Options ▸ Terms... (showTerms). Found on Xvfb: no visible keyboard focus on the See also buttons → black focus ring (also Show all). Xvfb checked: Find focused on open, "  CUT" 9 of 77, Cutoff first; xyzzy → No term has; Tab and Enter jump with "> " mark; Tab scrolls the list; Escape closes. 38 of 38.
- User: commit and do the crosscheck.

## Crosscheck after Phase 27

- Committed 27b (f3893c7).
- Code vs S7.10: Find rules (Terms.h), See also jump and "> " mark, Show all, keyboard (Find focused, Tab, Enter, Escape) as specified; as-built notes cover the focus ring and the focus after a jump. S6.11 lists Terms...; S3.2 has the computer-key fix; S9.8 Terms.html and the Start menu entry.
- Gaps fixed in the docs: S7.9 did not mention the term links → noted; S7.10 proposed terms not built as their own entry (sound, cent, wave names, noise colours, filter types, ADSR stages, damping, spread, redo) → noted where they are explained; Plan 27 → done.
- Tests: T-S7.10-terms (unit + Xvfb), T-S9.8-html pass; 38 of 38. Open: the installer Terms check runs in CI on Windows after the push.
- User pushed f3893c7 and 885466d. CI run 45574913048 (f3893c7) green on all four (Windows installer test checks the Terms Start menu entry and Terms.html). 885466d changes only architecture/ (paths-ignore: no run).

## Release 1.6.0

- User: commit (CI record) and prepare release 1.6.0.
- Upgrade check (T-S9.6-upgrade-1.6): 1.5.0 download on Xvfb → sound, cables, notes, OUT moved, closed → the 1.6.0 build: layout kept, /patch identical but for the four ensemble lines (Mix 0), round trip ok. (The 9P socket cannot live under the scratchpad: the path is longer than a Unix socket allows; the default runtime folder was used.)
- Screenshots retaken with the 1.6.0 title (String section, 1496 × 904); the Effects picture is now two rows (1496 × 552: ENSEMBLE makes the row wrap) and shows REVERB's Mix on the right. README and tutorial captions name ENSEMBLE. Downloads and test homes removed.
- Prepared: CMakeLists 1.6.0, Changes "## 1.6.0 (2026-11-25)" with the upgrade note, Spec S6.10, Plan row 1.6.0, Tests.
