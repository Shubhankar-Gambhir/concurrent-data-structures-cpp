// counter.hpp -- C++17 concurrent counter variants.
//
// Contract: each variant provides:
//     void    increment();   // atomically add 1
//     int64_t get() const;   // exact value once all writer threads have joined

#ifndef SYNC_BENCH_COUNTER_HPP
#define SYNC_BENCH_COUNTER_HPP

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <numeric>
#include <array>

// Cache-line size fallback for alignas padding.
#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_destructive_interference_size;
#else
    // Fallback to 64 bytes (the standard cache line for x86-64 and most modern CPUs)
    constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

namespace sync_bench {

class MutexCounter {
private:
    mutable std::mutex _m;
    int64_t _counter = 0;
public:
    void increment() {
        std::lock_guard l(_m);
        _counter++;
    }
    int64_t get() const { std::lock_guard l(_m); return _counter; }
};

class AtomicCounter {
private:
    std::atomic<int64_t> _counter{0};
public:
    void increment() { _counter.fetch_add(1, std::memory_order_relaxed); }
    int64_t get() const { return _counter.load(); }
};

class AtomicSeqCstCounter {
private:
    std::atomic<int64_t> _counter{0};
public:
    void increment() {_counter.fetch_add(1, std::memory_order_seq_cst);}
    int64_t get() const { return _counter.load(std::memory_order_seq_cst); }
};


template <typename T, std::size_t N = 64, bool Align = true>
class StripedCounter {
private:
    struct alignas(Align ? hardware_destructive_interference_size : alignof(T)) Cell { T counter; };
    
    std::array<Cell, N> _v;
    static inline std::atomic<int> _num_threads = 0;
    static thread_local inline int _id;
    static thread_local inline bool id_set;
public:
    void increment() { 
        if(!id_set) {
            _id = _num_threads.fetch_add(1, std::memory_order_seq_cst);
            id_set = true;
        }
        _v[_id % N].counter.increment(); 
    }
    int64_t get() const { 
        return std::accumulate(_v.begin(), _v.end(), 0LL, [](long long sum, const Cell& element) {
            return sum + element.counter.get();
        }); }
};

}  // namespace sync_bench

#endif  // SYNC_BENCH_COUNTER_HPP
