// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.5: rack layout of module panels, independent of JUCE so it can be unit-tested.
// Panels are numbered 0..n-1 and have a width in fader columns. The layout is a list of rows,
// each an ordered list of panels. A row may hold at most maxColumns columns; there are at most
// maxRows rows; empty rows are removed. Every panel appears exactly once.
#pragma once
#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lmw::ui {

class RackLayout {
public:
    using Rows = std::vector<std::vector<int>>;

    RackLayout(std::vector<int> panelColumns, Rows defaultRows, int maxColumns, int maxRows)
        : columns_(std::move(panelColumns)), default_(std::move(defaultRows)),
          maxColumns_(maxColumns), maxRows_(maxRows), rows_(default_) {}

    const Rows& rows() const { return rows_; }
    const Rows& defaultRows() const { return default_; }
    int maxRows() const { return maxRows_; }
    void reset() { rows_ = default_; }
    bool isDefault() const { return rows_ == default_; }

    // Row and position of a panel.
    std::pair<int, int> find(int panel) const {
        for (std::size_t r = 0; r < rows_.size(); ++r)
            for (std::size_t i = 0; i < rows_[r].size(); ++i)
                if (rows_[r][i] == panel) return {static_cast<int>(r), static_cast<int>(i)};
        return {-1, -1};
    }

    int rowColumns(const std::vector<int>& row) const {
        int c = 0;
        for (int p : row) c += columns_[static_cast<std::size_t>(p)];
        return c;
    }

    // The layout after moving `panel` to position `index` of row `row` (index counted without the
    // panel itself). With newRow, a new row is inserted at position `row` (0..rows) holding only the
    // panel. Returns nothing when the move is not allowed (row full, too many rows, bad index).
    std::optional<Rows> preview(int panel, int row, int index, bool newRow) const {
        auto [fromRow, fromIndex] = find(panel);
        if (fromRow < 0) return std::nullopt;
        Rows r = rows_;
        auto& src = r[static_cast<std::size_t>(fromRow)];
        src.erase(src.begin() + fromIndex);
        if (newRow) {
            if (row < 0 || row > static_cast<int>(r.size())) return std::nullopt;
            r.insert(r.begin() + row, std::vector<int>{panel});
        } else {
            if (row < 0 || row >= static_cast<int>(r.size())) return std::nullopt;
            auto& dst = r[static_cast<std::size_t>(row)];
            if (index < 0 || index > static_cast<int>(dst.size())) return std::nullopt;
            dst.insert(dst.begin() + index, panel);
            if (rowColumns(dst) > maxColumns_) return std::nullopt;
        }
        r.erase(std::remove_if(r.begin(), r.end(), [](const auto& v) { return v.empty(); }), r.end());
        if (static_cast<int>(r.size()) > maxRows_) return std::nullopt;
        return r;
    }

    bool move(int panel, int row, int index, bool newRow = false) {
        auto r = preview(panel, row, index, newRow);
        if (!r) return false;
        rows_ = std::move(*r);
        return true;
    }

    // Keyboard / menu moves (Spec S6.5: same result as dragging, without a mouse).
    bool moveLeft(int panel) {
        auto [r, i] = find(panel);
        return i > 0 && move(panel, r, i - 1);
    }
    bool moveRight(int panel) {
        auto [r, i] = find(panel);
        return r >= 0 && i + 1 < static_cast<int>(rows_[static_cast<std::size_t>(r)].size()) && move(panel, r, i + 1);
    }
    // Up/down: into the neighbouring row at the same position (or its end); if that row is full,
    // or there is none, into a new row of its own.
    bool moveUp(int panel) { return moveVertical(panel, -1); }
    bool moveDown(int panel) { return moveVertical(panel, +1); }

    // "0,1,2;7,8": rows separated by ';', panels by ','.
    std::string serialize() const {
        std::string s;
        for (std::size_t r = 0; r < rows_.size(); ++r) {
            if (r > 0) s += ';';
            for (std::size_t i = 0; i < rows_[r].size(); ++i) {
                if (i > 0) s += ',';
                s += std::to_string(rows_[r][i]);
            }
        }
        return s;
    }

    // Accepts only a complete, valid layout; otherwise the layout is left unchanged.
    bool parse(std::string_view text) {
        Rows r(1);
        std::vector<int> seen(columns_.size(), 0);
        int value = -1;
        auto flush = [&]() -> bool {
            if (value < 0 || value >= static_cast<int>(columns_.size()) || seen[static_cast<std::size_t>(value)]++) return false;
            r.back().push_back(value);
            value = -1;
            return true;
        };
        for (char c : text) {
            if (c >= '0' && c <= '9') {
                value = (value < 0 ? 0 : value * 10) + (c - '0');
                if (value > 1000) return false;
            } else if (c == ',') {
                if (!flush()) return false;
            } else if (c == ';') {
                if (!flush()) return false;
                r.emplace_back();
            } else {
                return false;
            }
        }
        if (!flush()) return false;
        if (static_cast<int>(r.size()) > maxRows_) return false;
        for (int s : seen) if (s != 1) return false;
        for (const auto& row : r) if (row.empty() || rowColumns(row) > maxColumns_) return false;
        rows_ = std::move(r);
        return true;
    }

private:
    bool moveVertical(int panel, int dir) {
        auto [r, i] = find(panel);
        if (r < 0) return false;
        const int target = r + dir;
        const bool alone = rows_[static_cast<std::size_t>(r)].size() == 1;
        if (target >= 0 && target < static_cast<int>(rows_.size())) {
            const int len = static_cast<int>(rows_[static_cast<std::size_t>(target)].size());
            if (move(panel, target, std::min(i, len))) return true;
            // Target row full: a panel alone in its row swaps rows with it; otherwise it gets a new
            // row of its own between the two.
            if (alone) return move(panel, dir < 0 ? target : target + 1, 0, true);
            return move(panel, dir < 0 ? r : r + 1, 0, true);
        }
        if (alone) return false;                                 // already a row of its own at the edge
        return move(panel, dir < 0 ? 0 : static_cast<int>(rows_.size()), 0, true);
    }

    std::vector<int> columns_;
    Rows default_;
    int maxColumns_, maxRows_;
    Rows rows_;
};

} // namespace lmw::ui
