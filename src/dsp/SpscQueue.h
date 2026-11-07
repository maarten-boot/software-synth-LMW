// SPDX-License-Identifier: AGPL-3.0-or-later
// Lock-free single-producer / single-consumer queue with fixed capacity (Spec S10.2).
// push() and pop() never lock or allocate, so either side may be the audio thread.
// Exactly one thread may push and exactly one (other) thread may pop.
#pragma once
#include <array>
#include <atomic>
#include <cstddef>

namespace lmw::dsp {

template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    // Returns false (and drops the item) when the queue is full.
    bool push(const T& item) noexcept {
        const auto w = write.load(std::memory_order_relaxed);
        if (w - read.load(std::memory_order_acquire) == Capacity) return false;
        slots[w & (Capacity - 1)] = item;
        write.store(w + 1, std::memory_order_release);
        return true;
    }

    // Returns false when the queue is empty.
    bool pop(T& item) noexcept {
        const auto r = read.load(std::memory_order_relaxed);
        if (r == write.load(std::memory_order_acquire)) return false;
        item = slots[r & (Capacity - 1)];
        read.store(r + 1, std::memory_order_release);
        return true;
    }

    // Consumer side: discard everything currently queued.
    void clear() noexcept { T dummy; while (pop(dummy)) {} }

private:
    std::array<T, Capacity> slots{};
    alignas(64) std::atomic<std::size_t> write{0};
    alignas(64) std::atomic<std::size_t> read{0};
};

// A note event passed between the GUI thread and the audio thread.
struct NoteEvent {
    enum class Kind : unsigned char { On, Off, AllOff } kind = Kind::Off;
    unsigned char channel = 1;
    unsigned char note = 0;
    float velocity = 0.0f;
};

} // namespace lmw::dsp
