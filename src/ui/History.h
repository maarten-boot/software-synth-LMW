// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.14: the undo / redo history, independent of JUCE so it can be unit-tested. The sound is a map
// of keys to texts (a parameter, an input's cables, the schematic, ...). A step is made by begin(name,
// now) ... end(now): it keeps only the keys whose text changed, with the text before and after (a key
// that did not exist has the text ""). Steps nest: only the outermost begin/end makes a step, so one
// action that calls others (an Assistant answer setting parameters and cables) is one step. A step that
// changes nothing is not kept. At most `depth` steps (the oldest go); a new step drops the redo steps.
#pragma once
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace lmw::ui {

class History {
public:
    using State = std::map<std::string, std::string>;
    struct Step {
        std::string name;
        State before, after;          // the changed keys only
    };

    explicit History(std::size_t depth = 100) : depth_(depth) {}

    void begin(const std::string& name, const State& now) {
        if (open_++ == 0) { name_ = name; start_ = now; }
    }
    // True when a step was kept.
    bool end(const State& now) {
        if (open_ == 0 || --open_ > 0) return false;
        Step s{name_, {}, {}};
        const auto text = [](const State& st, const std::string& k) {
            const auto it = st.find(k);
            return it == st.end() ? std::string() : it->second;
        };
        const auto compare = [&](const State& keys) {
            for (const auto& kv : keys) {
                const auto a = text(start_, kv.first), b = text(now, kv.first);
                if (a != b) { s.before[kv.first] = a; s.after[kv.first] = b; }
            }
        };
        compare(start_);
        compare(now);
        start_.clear();
        if (s.after.empty()) return false;
        steps_.resize(current_);                       // a new step drops the redo steps
        steps_.push_back(std::move(s));
        if (steps_.size() > depth_) steps_.erase(steps_.begin(), steps_.begin() + static_cast<std::ptrdiff_t>(steps_.size() - depth_));
        current_ = steps_.size();
        return true;
    }
    bool recording() const { return open_ > 0; }
    void rename(const std::string& name) { if (open_ > 0) name_ = name; }   // the open step's name

    bool canUndo() const { return open_ == 0 && current_ > 0; }
    bool canRedo() const { return open_ == 0 && current_ < steps_.size(); }
    std::string undoName() const { return canUndo() ? steps_[current_ - 1].name : std::string(); }
    std::string redoName() const { return canRedo() ? steps_[current_].name : std::string(); }
    // The step to undo (apply its `before`) or redo (apply its `after`); nothing when there is none.
    std::optional<Step> undo() {
        if (!canUndo()) return std::nullopt;
        return steps_[--current_];
    }
    std::optional<Step> redo() {
        if (!canRedo()) return std::nullopt;
        return steps_[current_++];
    }
    void clear() { steps_.clear(); current_ = 0; }

    // 9P /history: one line per step, oldest first; "> " before the step Undo would take back, "  " else.
    std::string lines() const {
        std::string out;
        for (std::size_t i = 0; i < steps_.size(); ++i) out += (i + 1 == current_ ? "> " : "  ") + steps_[i].name + "\n";
        return out;
    }
    std::size_t size() const { return steps_.size(); }

private:
    std::size_t depth_;
    std::vector<Step> steps_;
    std::size_t current_ = 0;          // steps_[0, current_) are done; the rest can be redone
    int open_ = 0;
    std::string name_;
    State start_;
};

} // namespace lmw::ui
