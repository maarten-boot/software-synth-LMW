// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S10.2b: lock-free SPSC queue — order, capacity, empty/full, and a two-thread stress run.
#include "dsp/SpscQueue.h"
#include <cstdio>
#include <thread>

using lmw::dsp::SpscQueue;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    static_assert(std::atomic<std::size_t>::is_always_lock_free, "indices must be lock-free");

    SpscQueue<int, 4> q;
    int v = -1;
    CHECK(!q.pop(v));                                   // empty
    for (int i = 0; i < 4; ++i) CHECK(q.push(i));
    CHECK(!q.push(99));                                 // full: dropped
    for (int i = 0; i < 4; ++i) { CHECK(q.pop(v)); CHECK(v == i); }   // FIFO order
    CHECK(!q.pop(v));
    for (int round = 0; round < 10; ++round) {          // wrap-around
        CHECK(q.push(round)); CHECK(q.pop(v)); CHECK(v == round);
    }
    q.push(1); q.push(2); q.clear(); CHECK(!q.pop(v));

    // Stress: producer and consumer threads; every value arrives once, in order.
    static SpscQueue<int, 64> s;
    constexpr int total = 1'000'000;
    std::thread producer([] { for (int i = 0; i < total; ++i) while (!s.push(i)) std::this_thread::yield(); });
    int expected = 0;
    bool inOrder = true;
    while (expected < total) {
        int x;
        if (s.pop(x)) { inOrder = inOrder && (x == expected); ++expected; }
        else std::this_thread::yield();
    }
    producer.join();
    CHECK(inOrder);

    std::printf(failures ? "%d failure(s)\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
