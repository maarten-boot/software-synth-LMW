// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.5-layout: rack layout model (moves, row capacity, keyboard moves, save/restore).
#include "ui/RackLayout.h"
#include <cstdio>

using lmw::ui::RackLayout;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

// Same panels as the editor: POLY VCO MIXER VCF VCA LFO OUT | ENV1 ENV2 (widths in fader columns).
static RackLayout make() {
    return RackLayout({1, 2, 2, 3, 1, 2, 1, 4, 4}, {{0, 1, 2, 3, 4, 5, 6}, {7, 8}}, 12, 4);
}

int main() {
    {   // default and round trip
        auto r = make();
        CHECK(r.serialize() == "0,1,2,3,4,5,6;7,8");
        CHECK(r.isDefault());
        CHECK(r.parse("7,8;0,1,2,3,4,5,6"));
        CHECK(r.serialize() == "7,8;0,1,2,3,4,5,6");
        r.reset();
        CHECK(r.isDefault());
    }
    {   // reorder within a row: VCF before VCO
        auto r = make();
        CHECK(r.move(3, 0, 1));
        CHECK(r.serialize() == "0,3,1,2,4,5,6;7,8");
    }
    {   // between rows: row 0 is full (12 columns), so ENV 1 cannot join it ...
        auto r = make();
        CHECK(!r.move(7, 0, 0));
        CHECK(r.isDefault());
        // ... but LFO can move down next to the envelopes, and then ENV 1 still does not fit in row 0
        CHECK(r.move(5, 1, 1));
        CHECK(r.serialize() == "0,1,2,3,4,6;7,5,8");
        CHECK(r.move(4, 1, 3));          // VCA to the end of row 1
        CHECK(r.serialize() == "0,1,2,3,6;7,5,8,4");
    }
    {   // new rows, empty rows removed, at most 4 rows
        auto r = make();
        CHECK(r.move(8, 2, 0, true));    // ENV 2 into a new third row
        CHECK(r.serialize() == "0,1,2,3,4,5,6;7;8");
        CHECK(r.move(6, 0, 0, true));    // OUT into a new first row
        CHECK(r.serialize() == "6;0,1,2,3,4,5;7;8");
        CHECK(!r.move(0, 4, 0, true));   // a fifth row is not allowed
        CHECK(r.move(8, 2, 1));          // ENV 2 back next to ENV 1: its row disappears
        CHECK(r.serialize() == "6;0,1,2,3,4,5;7,8");
        CHECK(!r.move(0, 9, 0));         // bad row
        CHECK(!r.move(0, 1, 9));         // bad index
    }
    {   // keyboard moves
        auto r = make();
        CHECK(!r.moveLeft(0));           // already first
        CHECK(r.moveRight(0));
        CHECK(r.serialize() == "1,0,2,3,4,5,6;7,8");
        CHECK(!r.moveRight(6));          // already last
        CHECK(r.moveDown(1));            // VCO (position 0) to row 1 position 0
        CHECK(r.serialize() == "0,2,3,4,5,6;1,7,8");
        CHECK(r.moveUp(1));
        CHECK(r.serialize() == "1,0,2,3,4,5,6;7,8");
        CHECK(r.moveUp(7));              // row 0 is full: ENV 1 gets a new row between the two
        CHECK(r.serialize() == "1,0,2,3,4,5,6;7;8");
        CHECK(r.moveUp(7));              // alone, row above full: the rows swap
        CHECK(r.serialize() == "7;1,0,2,3,4,5,6;8");
        CHECK(!r.moveUp(7));             // alone in the top row: nowhere further
        CHECK(r.moveDown(7));            // swap back
        CHECK(r.serialize() == "1,0,2,3,4,5,6;7;8");
        CHECK(r.moveDown(7));            // into ENV 2's row, same position (0)
        CHECK(r.serialize() == "1,0,2,3,4,5,6;7,8");
    }
    {   // restoring a saved layout rejects anything incomplete or invalid
        auto r = make();
        CHECK(!r.parse(""));
        CHECK(!r.parse("0,1,2,3,4,5,6;7"));          // ENV 2 missing
        CHECK(!r.parse("0,1,2,3,4,5,6;7,8,8"));      // duplicate
        CHECK(!r.parse("0,1,2,3,4,5,6;7,8,9"));      // unknown panel
        CHECK(!r.parse("0,1,2,3,4,5,6,7;8"));        // row too wide
        CHECK(!r.parse("0,1;;2,3,4,5,6;7,8"));       // empty row
        CHECK(!r.parse("0;1;2;3;4,5,6,7,8"));        // too many rows / too wide
        CHECK(!r.parse("0,1,2,3,4,5,6;7,8x"));
        CHECK(r.isDefault());
        CHECK(r.parse("0;1,2,3,4,5,6;7;8"));
    }
    if (failures == 0) std::printf("T-S6.5-layout: all passed\n");
    return failures == 0 ? 0 : 1;
}
