# Limit Order Book & Matching Engine (C++23)

A high-performance limit order book and matching engine implementation in C++23, featuring a custom slab allocator and price-time priority matching.

## Quick Start

```bash
# Build using CMake
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run the example
./examples/example_basic

# Run benchmarks
cd benchmarks && ./benchmarks
```

Or use the manual build script:
```bash
./build.sh
./build_manual/example_basic
```

## Features

- **Price-Time Priority Matching**: Orders matched by price first, then time (FIFO)
- **Custom Slab Allocator**: Pre-allocated memory pools for zero-allocation order management
- **Multiple Order Types**: Limit, Market, IOC (Immediate or Cancel), FOK (Fill or Kill)
- **High Performance**: Optimized for low-latency trading systems
- **C++23 Features**: 
  - `std::print` / `std::println` for formatted output
  - `std::optional` monadic operations (`.transform()`, `.and_then()`, `.or_else()`)
  - `std::ranges::to` for range-to-container conversion
  - `std::source_location` for better error tracking
- **Testing & Benchmarks**: Unit tests (Catch2) and performance benchmarks (Google Benchmark)

## Architecture

### Components

1. **Order Types** (`include/types.hpp`)
   - Order structure with price, quantity, side, type, and status
   - Trade structure for executed trades
   - Timestamp tracking for price-time priority

2. **Slab Allocator** (`include/allocator/slab_allocator.hpp`)
   - Pre-allocates memory pools (slabs)
   - O(1) allocation/deallocation with free list reuse
   - Lock-free for maximum performance

3. **Order Book** (`include/order_book.hpp`)
   - Maintains bid/ask price levels using `std::map` (O(log n) lookup)
   - Price levels contain linked lists of orders (FIFO, O(1) insertion)
   - O(1) order lookup via `std::unordered_map` for cancellation/modification
   - Market depth queries

4. **Matching Engine** (`include/matching_engine.hpp`)
   - Processes incoming orders and matches based on price-time priority
   - Generates trades and supports all order types

## Building

### Requirements

- C++23 compatible compiler (GCC 13+, Clang 16+, MSVC 19.30+)
- CMake 3.20+ (optional - manual build script available)

### Build Instructions

**Using CMake (recommended):**
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**Manual Build:**
```bash
./build.sh
```

## Performance

### Time Complexity

- **Order Insertion**: O(log n) price level lookup + O(1) order insertion
- **Order Cancellation**: O(1) hash map lookup
- **Best Bid/Ask Lookup**: O(1) constant time
- **Matching**: O(k) where k is number of price levels matched

### Benchmark Results

On modern hardware, typical performance:
- **Allocator**: ~34 ns allocation, ~1.5 ns deallocation
- **Order Book**: ~123 ns add order, ~13 ns best bid/ask lookup, ~47 ns cancel
- **Matching**: ~8-15M orders/sec throughput across order types
- **Scalability**: Constant-time best bid/ask lookup even with 10,000+ orders

Run benchmarks for detailed results:
```bash
cd build/benchmarks && ./benchmarks
```

## Design Decisions

### Price-Time Priority

Orders matched using:
1. **Price Priority**: Best price wins (highest bid, lowest ask)
2. **Time Priority**: Within same price, first-in-first-out (FIFO)

### Slab Allocator

- Zero-allocation order creation after initial setup
- Cache-friendly memory layout
- Fast deallocation via free list
- Memory reuse for better performance

### Data Structures

- **Price Levels**: `std::map` for O(log n) price lookup
- **Orders per Level**: Doubly-linked list for O(1) insertion and FIFO ordering
- **Order Lookup**: `std::unordered_map` for O(1) cancellation/modification

## Testing

Run tests with CMake:
```bash
cd build
ctest --output-on-failure
```

Tests cover order book operations, matching logic, order types, allocator functionality, and edge cases.

## Future Enhancements

- [ ] More order types (stop orders, trailing stops)
- [ ] Order book visualization
- [ ] Network protocol for order submission
- [ ] More sophisticated matching algorithms

## Contributing

This is a portfolio project, but suggestions and improvements are welcome!

## Why This Project?

This implementation demonstrates:
- Low-latency C++ programming
- Memory management (custom allocators)
- Algorithms and data structures
- Trading systems and market microstructure
- Performance optimization
- Modern C++ (C++23)
