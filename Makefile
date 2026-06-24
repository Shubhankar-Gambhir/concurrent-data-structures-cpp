CXX      ?= g++
CXXFLAGS  = -std=c++17 -O2 -Wall -Wextra -pthread

.PHONY: all test test-modern bench bench-modern clean

all: build/test_counter build/bench_counter

# --- Counter: correctness tests ---

build/test_counter: counter/examples/test_counter.cpp counter/examples/counter.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

test: build/test_counter
	./build/test_counter

build/test_counter_modern: counter/examples/test_counter_modern.cpp counter/examples/counter_modern.hpp | build
	$(CXX) -std=c++20 -O2 -Wall -Wextra -pthread $< -o $@

test-modern: build/test_counter_modern
	./build/test_counter_modern

# --- Counter: benchmarks ---

build/bench_counter: counter/benchmarks/bench_counter.cpp counter/examples/counter.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

bench: build/bench_counter
	./build/bench_counter

build/bench_counter_modern: counter/benchmarks/bench_counter.cpp counter/examples/counter.hpp counter/examples/counter_modern.hpp | build
	$(CXX) -std=c++20 -O2 -Wall -Wextra -pthread $< -o $@

bench-modern: build/bench_counter_modern
	./build/bench_counter_modern

build:
	mkdir -p build

clean:
	rm -rf build
