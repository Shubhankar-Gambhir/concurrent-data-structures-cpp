// counter_modern.hpp -- C++20 concurrent counter variants.
//
// Same contract as counter.hpp:
//     void    increment();   // atomically add 1
//     int64_t get() const;   // exact value once all writer threads have joined
//
// C++20 features exercised: std::atomic_ref, atomic::wait/notify_one.

#ifndef SYNC_BENCH_COUNTER_MODERN_HPP
#define SYNC_BENCH_COUNTER_MODERN_HPP

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <array>
#include <numeric>

namespace sync_bench {

inline constexpr std::size_t kCacheLineModern = 64;

// AtomicRefCounter -- std::atomic_ref over a plain int64_t.
// Measures whether atomic_ref produces the same codegen as std::atomic.
class AtomicRefCounter {
public:
    void increment() {
        std::atomic_ref<int64_t> ref(_counter);
        ref.fetch_add(1, std::memory_order_relaxed);
    }
    int64_t get() const {
        std::atomic_ref<int64_t> ref(const_cast<int64_t&>(_counter));
        return ref.load(std::memory_order_relaxed);
    }

private:
    alignas(std::atomic_ref<int64_t>::required_alignment) int64_t _counter = 0;
};

// WaitNotifyCounter -- fetch_add + notify_one on every increment.
// Measures the overhead of unconditional notify_one in a hot loop.
class WaitNotifyCounter {
public:
    void increment() {
        _counter.fetch_add(1, std::memory_order_relaxed);
        _counter.notify_one();
    }
    
    int64_t get() const {
        return _counter.load(std::memory_order_relaxed);
    }

private:
    std::atomic<int64_t> _counter{0};
};

// ModernStripedCounter -- LongAdder with atomic_ref cells (plain int64_t storage).
// Tests whether atomic_ref introduces overhead vs a permanent std::atomic.
template <std::size_t N = 64, bool Align = true>
class ModernStripedCounter {
public:
    void increment() {
        std::atomic_ref<int64_t> ref(_v[slot()].value);
        ref.fetch_add(1, std::memory_order_relaxed);
    }
    int64_t get() const {
        return std::accumulate(_v.begin(), _v.end(), 0LL, 
            [](long long sum, const Cell& element) {
            std::atomic_ref<int64_t> ref(const_cast<int64_t&> (element.value));
            return sum + ref.load(std::memory_order_relaxed);
        });
    }

private:
    struct alignas(Align ? kCacheLineModern : alignof(int64_t)) Cell {
        int64_t value = 0;
    };

    std::array<Cell, N> _v{};

    static std::size_t slot() {
        static std::atomic<std::size_t> next{0};
        thread_local std::size_t s = next.fetch_add(1, std::memory_order_relaxed);
        return s % N;
    }
};

}  // namespace sync_bench

#endif  // SYNC_BENCH_COUNTER_MODERN_HPP
