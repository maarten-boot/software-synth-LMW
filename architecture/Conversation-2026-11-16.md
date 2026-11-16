# Conversation 2026-11-16

**User:** status ?
Status given: all work up to 3cf4e8f pushed and green on all four platforms; open: Phase 7 (Kukel project), Windows/macOS DAW tests, faster schematic keyboard navigation (optional).

**User:** commit the docs
Committed the 2026-11-14 CI log lines and this file (docs only: CI skipped by paths-ignore).

**User:** i have not yet used the keyboard navigation on the schematic, please show what is supported there >
(Trailing ">" read as a stray "?".) Explained the Schematic tab keys from SchematicView::keyPressed (S6.7): arrows walk all 38 ports block by block (wrapping), Shift+arrows move the block, Enter / Shift+F10 open the cable menu, Delete/Backspace clear an input, Escape cancels a drag, other keys play notes.

**User:** make up/down jump to the next symbol
Done: Up/Down go to the first port of the next / previous symbol in the port order (wrapping); Left/Right still step port by port; Shift+arrows still move the symbol. Helper sc::blockJump (Schematic.h) with a test (T-S6.7i); tab description text updated. Ubuntu 24.04 container: 20/20 ctests. Xvfb: Down ×6 from LFO 1 → MIX in1; Right ×2, Down → VCF 1 in.

**User:** pushed, wait for CI
CI: run 43848898225 for d476da2 (schematic Up/Down): all four platforms green.
