cmake_minimum_required(VERSION 3.20)

add_executable(example_basic
    basic_example.cpp
)

target_link_libraries(example_basic PRIVATE lob_cpp)

