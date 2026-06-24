# Concurrent Data Structures and Algorithms in C++

Measure the real cost of concurrency primitives. Each data structure lives in its
own directory with correctness tests, a multi-threaded benchmark harness, and
C++17 baseline + C++20 modern variants.

Portable: Linux (with core pinning) and macOS (Apple Silicon / x86).

## Data structures

### [`counter/`](counter/) -- Concurrent counter (current)

Four C++17 counter implementations (mutex, atomic-relaxed, atomic-seq_cst,
LongAdder-style striped), plus three C++20 variants (`std::atomic_ref`,
`atomic::wait/notify`). Benchmarked across thread counts with cache-line
padding as a controlled variable.

## Key findings (Intel Xeon Gold 6130, GCC 11, -O2)

| Variant (32 threads) | ops/sec | vs AtomicCounter |
|-----------------------|---------|-------------------|
| Striped\<Atomic, aligned\> | 2,459M | 85x faster |
| Striped\<Atomic, **unaligned**\> | 79M | 2.7x faster |
| AtomicCounter (relaxed) | 29M | baseline |
| AtomicSeqCstCounter | 29M | identical on x86 |
| MutexCounter | 6.9M | 4.2x slower |

- **False sharing penalty: 31x** (aligned vs unaligned striped counter)
- **relaxed vs seq_cst: identical on x86** (both compile to `lock xadd`)
- **`std::atomic_ref` (C++20): zero overhead** vs `std::atomic`
- **`notify_one`: 48% overhead** when called unconditionally in a hot loop

## Build and run

```bash
# C++17 baseline
make test          # correctness smoke test (single + multi-threaded)
make bench         # benchmark all variants

# C++20 (atomic_ref, wait/notify -- needs GCC 11+ or Clang 14+)
make test-modern   # correctness smoke test for C++20 variants
make bench-modern  # benchmark all variants including C++20
```

## Counter variants

### C++17 baseline (`counter/examples/counter.hpp`)

| Type | Implementation |
|------|---------------|
| `MutexCounter` | `std::mutex` + `lock_guard`, RAII |
| `AtomicCounter` | `std::atomic<int64_t>`, relaxed `fetch_add` |
| `AtomicSeqCstCounter` | same, seq_cst -- measures memory-order delta |
| `StripedCounter<T, N, Align>` | N padded cells, one per thread (LongAdder pattern) |

### C++20 variants (`counter/examples/counter_modern.hpp`)

| Type | C++20 feature |
|------|--------------|
| `AtomicRefCounter` | `std::atomic_ref` over a plain `int64_t` |
| `WaitNotifyCounter` | `atomic::wait` / `notify_one` |
| `ModernStripedCounter<N, Align>` | `atomic_ref` cells instead of `std::atomic` wrapper |

## Benchmark methodology

- Synchronized start (atomic go-flag; all threads begin together)
- Core pinning on Linux (`pthread_setaffinity_np`; no-op on macOS)
- Post-run invariant check (`get() == threads * iterations`)
- Warmup run before measured runs
- Median of 5 runs with min/max spread reported
- `volatile` sink to defeat dead-code elision

## Hardware tested

- Intel Xeon Gold 6130 @ 2.10 GHz (2x16 cores, 64 logical CPUs, Skylake)

## License

MIT
