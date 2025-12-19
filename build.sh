#!/bin/bash
set -e

# Simple build script for lob_cpp
# Requires: C++23 compatible compiler (GCC 13+, Clang 16+)

BUILD_DIR="build_manual"
mkdir -p "$BUILD_DIR"

echo "Building lob_cpp..."

# Compile source files
g++ -std=c++23 -I include -O3 -Wall -Wextra -c src/slab_allocator.cpp -o "$BUILD_DIR/slab_allocator.o"
g++ -std=c++23 -I include -O3 -Wall -Wextra -c src/order_book.cpp -o "$BUILD_DIR/order_book.o"
g++ -std=c++23 -I include -O3 -Wall -Wextra -c src/matching_engine.cpp -o "$BUILD_DIR/matching_engine.o"

# Create static library
ar rcs "$BUILD_DIR/liblob_cpp.a" "$BUILD_DIR/slab_allocator.o" "$BUILD_DIR/order_book.o" "$BUILD_DIR/matching_engine.o"

# Build example
g++ -std=c++23 -I include -O3 -Wall -Wextra examples/basic_example.cpp -L"$BUILD_DIR" -llob_cpp -o "$BUILD_DIR/example_basic"

echo "Build complete! Run with: $BUILD_DIR/example_basic"
