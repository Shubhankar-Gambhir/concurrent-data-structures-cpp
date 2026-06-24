// test_counter_modern.cpp -- correctness smoke test for C++20 counter variants.
//
// Build:  make test-modern
// Run:    ./build/test_counter_modern

#include "counter_modern.hpp"

#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

using namespace sync_bench;

static int g_failures = 0;

template <typename Counter>
void test_single(const char* name, int64_t increments) {
    Counter c;
    for (int64_t i = 0; i < increments; ++i)
        c.increment();

    const int64_t got = c.get();
    const bool ok = (got == increments);
    if (!ok) ++g_failures;
    std::printf("  [%s] single-thread: %ld increments -> get()=%ld  %s\n",
                name, static_cast<long>(increments), static_cast<long>(got),
                ok ? "OK" : "FAIL");
}

template <typename Counter>
void test_multi(const char* name, int threads, int64_t per_thread) {
    Counter c;
    std::vector<std::thread> pool;
    pool.reserve(static_cast<std::size_t>(threads));

    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&c, per_thread] {
            for (int64_t i = 0; i < per_thread; ++i)
                c.increment();
        });
    }
    for (auto& th : pool)
        th.join();

    const int64_t expected = static_cast<int64_t>(threads) * per_thread;
    const int64_t got = c.get();
    const bool ok = (got == expected);
    if (!ok) ++g_failures;
    std::printf("  [%s] %d threads x %ld -> get()=%ld (expected %ld)  %s\n",
                name, threads, static_cast<long>(per_thread),
                static_cast<long>(got), static_cast<long>(expected),
                ok ? "OK" : "FAIL");
}

int main() {
    constexpr int64_t kSingle    = 1'000'000;
    constexpr int     kThreads   = 8;
    constexpr int64_t kPerThread = 500'000;

    std::printf("sizeof/alignof sanity:\n");
    std::printf("  AtomicRefCounter       size = %zu bytes\n", sizeof(AtomicRefCounter));
    std::printf("  WaitNotifyCounter      size = %zu bytes\n", sizeof(WaitNotifyCounter));
    std::printf("  ModernStripedCounter   size = %zu bytes\n", sizeof(ModernStripedCounter<>));

    std::printf("\nSingle-threaded correctness:\n");
    test_single<AtomicRefCounter>     ("AtomicRefCounter  ", kSingle);
    test_single<WaitNotifyCounter>    ("WaitNotifyCounter ", kSingle);
    test_single<ModernStripedCounter<>>("ModernStriped     ", kSingle);

    std::printf("\nMulti-threaded invariant check (%d threads x %ld each):\n",
                kThreads, static_cast<long>(kPerThread));
    test_multi<AtomicRefCounter>     ("AtomicRefCounter  ", kThreads, kPerThread);
    test_multi<WaitNotifyCounter>    ("WaitNotifyCounter ", kThreads, kPerThread);
    test_multi<ModernStripedCounter<>>("ModernStriped     ", kThreads, kPerThread);

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES PRESENT",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
