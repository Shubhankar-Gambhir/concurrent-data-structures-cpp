// test_counter.cpp -- correctness smoke test for the C++17 counter variants.
// Build: make test

#include "counter.hpp"

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

    using StripedAtomic    = StripedCounter<AtomicCounter>;
    using StripedSeqCst    = StripedCounter<AtomicSeqCstCounter>;
    using StripedMutex     = StripedCounter<MutexCounter>;
    using StripedNoAlign   = StripedCounter<AtomicCounter, 64, false>;

    std::printf("sizeof/alignof sanity:\n");
    std::printf("  StripedCounter<AtomicCounter>          size = %zu bytes\n", sizeof(StripedAtomic));
    std::printf("  StripedCounter<AtomicCounter,64,false> size = %zu bytes\n", sizeof(StripedNoAlign));

    std::printf("\nSingle-threaded correctness:\n");
    test_single<MutexCounter>      ("MutexCounter      ", kSingle);
    test_single<AtomicCounter>     ("AtomicCounter     ", kSingle);
    test_single<AtomicSeqCstCounter>("AtomicSeqCst      ", kSingle);
    test_single<StripedAtomic>     ("Striped<Atomic>   ", kSingle);
    test_single<StripedSeqCst>     ("Striped<SeqCst>   ", kSingle);
    test_single<StripedMutex>      ("Striped<Mutex>    ", kSingle);
    test_single<StripedNoAlign>    ("Striped<NoAlign>  ", kSingle);

    std::printf("\nMulti-threaded invariant check (%d threads x %ld each):\n",
                kThreads, static_cast<long>(kPerThread));
    test_multi<MutexCounter>      ("MutexCounter      ", kThreads, kPerThread);
    test_multi<AtomicCounter>     ("AtomicCounter     ", kThreads, kPerThread);
    test_multi<AtomicSeqCstCounter>("AtomicSeqCst      ", kThreads, kPerThread);
    test_multi<StripedAtomic>     ("Striped<Atomic>   ", kThreads, kPerThread);
    test_multi<StripedSeqCst>     ("Striped<SeqCst>   ", kThreads, kPerThread);
    test_multi<StripedMutex>      ("Striped<Mutex>    ", kThreads, kPerThread);
    test_multi<StripedNoAlign>    ("Striped<NoAlign>  ", kThreads, kPerThread);

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES PRESENT",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
