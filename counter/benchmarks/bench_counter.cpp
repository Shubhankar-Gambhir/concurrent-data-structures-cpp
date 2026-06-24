// bench_counter.cpp -- multi-threaded benchmark harness for counter variants.
//
// Measures throughput (ops/sec) and per-op latency across thread counts.
// Build: make bench (C++17) or make bench-modern (C++20)
// Run:   ./build/bench_counter

#include "../examples/counter.hpp"
#ifdef __cplusplus
#if __cplusplus >= 202002L
#include "../examples/counter_modern.hpp"
#define HAS_MODERN 1
#endif
#endif

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
#include <algorithm>

using namespace sync_bench;

static constexpr long ITERATIONS  = 1'000'000L;
static constexpr int  RUNS        = 5;
static constexpr int  THREAD_COUNTS[] = {1, 2, 4, 8, 16, 32};

static volatile int64_t sink;

// Pin the calling thread to logical core `cpu`.
// Linux: pthread_setaffinity_np. macOS: no hard pinning available (no-op).
void pin_thread([[maybe_unused]] int cpu) {
#ifdef __linux__
    if ((unsigned) cpu >= std::thread::hardware_concurrency()) return;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

// Run `num_threads` threads, each doing `iters` increments on a shared Counter.
// Returns elapsed seconds.
// Requirements:
//   - Each thread pinned to a distinct core (thread i -> core i)
//   - All threads must start incrementing at the same instant (synchronized start)
//   - After all threads join, verify get() == num_threads * iters (invariant check)
//   - Consume the final value into `sink` to defeat elision
template <typename Counter>
double bench_one(int num_threads, long iters) {
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    Counter C;
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([i, iters, &C, &ready, &go]() {
            pin_thread(i);
            ready.fetch_add(1, std::memory_order_release);
            while (!go.load(std::memory_order_acquire)) {}
            for (long j = 0; j < iters; j++) C.increment();
        });
    }

    while (ready.load(std::memory_order_acquire) < num_threads) {}
    auto start = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);

    for (auto& t : threads)
        t.join();

    auto end = std::chrono::steady_clock::now();

    sink = C.get();
    if (C.get() != static_cast<int64_t>(num_threads) * iters) {
        std::printf("  INVARIANT FAILED: got %ld, expected %ld\n",
                    static_cast<long>(C.get()),
                    static_cast<long>(static_cast<int64_t>(num_threads) * iters));
        return 0.0;
    }

    return std::chrono::duration<double>(end - start).count();
}

// Run bench_one at each thread count, RUNS times each.
// For each thread count, report: median ops/sec, min, max, and median ns/op.
template <typename Counter>
void run_sweep(const char* name) {
    std::printf("\n=== %s ===\n", name);
    std::printf("  %6s  %14s  %14s  %14s  %10s\n",
                "threads", "median ops/s", "min ops/s", "max ops/s", "ns/op");

    for (int t : THREAD_COUNTS) {
        // Warmup run (discard) to get caches and branch predictors hot
        bench_one<Counter>(t, ITERATIONS);

        std::array<double, RUNS> ops_per_sec;
        for (int r = 0; r < RUNS; r++) {
            double secs = bench_one<Counter>(t, ITERATIONS);
            long total_ops = static_cast<long>(t) * ITERATIONS;
            ops_per_sec[r] = static_cast<double>(total_ops) / secs;
        }
        std::sort(ops_per_sec.begin(), ops_per_sec.end());
        double min = ops_per_sec[0];
        double max = ops_per_sec[RUNS - 1];
        double median = 0.5 * ops_per_sec[(RUNS - 1) / 2] + 0.5 * ops_per_sec[RUNS / 2];
        double ns_per_op = 1e9 / median;
        std::printf("  %6d  %14.0f  %14.0f  %14.0f  %10.2f\n",
                t, median, min, max, ns_per_op);
    }
}

int main() {
    std::printf("Counter benchmark: %ld iterations/thread, %d runs, median reported\n",
                ITERATIONS, RUNS);

    run_sweep<MutexCounter>("MutexCounter");
    run_sweep<AtomicCounter>("AtomicCounter (relaxed)");
    run_sweep<AtomicSeqCstCounter>("AtomicSeqCstCounter");
    run_sweep<StripedCounter<AtomicCounter>>("Striped<Atomic,aligned>");
    run_sweep<StripedCounter<AtomicSeqCstCounter>>("Striped<SeqCst,aligned>");
    run_sweep<StripedCounter<MutexCounter>>("Striped<Mutex,aligned>");
    run_sweep<StripedCounter<AtomicCounter, 64, false>>("Striped<Atomic,unaligned>");
    run_sweep<StripedCounter<AtomicSeqCstCounter, 64, false>>("Striped<SeqCst,unaligned>");
    run_sweep<StripedCounter<MutexCounter, 64, false>>("Striped<Mutex,unaligned>");

#ifdef HAS_MODERN
    std::printf("\n\n--- C++20 variants ---\n");
    run_sweep<AtomicRefCounter>("AtomicRefCounter (C++20)");
    run_sweep<WaitNotifyCounter>("WaitNotifyCounter (C++20)");
    run_sweep<ModernStripedCounter<>>("ModernStriped<aligned> (C++20)");
    run_sweep<ModernStripedCounter<64, false>>("ModernStriped<unaligned> (C++20)");
#endif

    return 0;
}
