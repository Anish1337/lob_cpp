cmake_minimum_required(VERSION 3.20)

find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    FetchContent_GetProperties(googlebenchmark)
    if(NOT googlebenchmark_POPULATED)
        # Disable GoogleTest to avoid CMake compatibility issues
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
        FetchContent_Populate(googlebenchmark)
        add_subdirectory(${googlebenchmark_SOURCE_DIR} ${googlebenchmark_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
    set(benchmark_LIBRARIES benchmark::benchmark benchmark::benchmark_main)
endif()

add_executable(benchmarks
    benchmark_order_book.cpp
    benchmark_matching.cpp
    benchmark_allocator.cpp
)

target_link_libraries(benchmarks PRIVATE lob_cpp ${benchmark_LIBRARIES})

