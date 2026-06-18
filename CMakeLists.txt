cmake_minimum_required(VERSION 3.20)
project(cordyceps LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O3 -march=native)
endif()

# Source files (for reference)
# Built via add_executable below

# Test files (added incrementally as tests written)
set(UNIT_TESTS
    tests/unit/test_bitboard.cpp
    tests/unit/test_board.cpp
    tests/unit/test_movegen.cpp
)

# GoogleTest via FetchContent
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
FetchContent_MakeAvailable(googletest)

# Main executable
add_executable(cordyceps
    src/common/bitboard.cpp
    src/engine/board.cpp
    src/engine/movegen.cpp
    src/io/protocol.cpp
    src/main.cpp
)
target_include_directories(cordyceps PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Test executable
add_executable(cordyceps_tests ${UNIT_TESTS} 
    src/common/bitboard.cpp
    src/engine/board.cpp
    src/engine/movegen.cpp
)
target_include_directories(cordyceps_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cordyceps_tests GTest::gtest GTest::gmock GTest::gtest_main)

# CTest
enable_testing()
include(GoogleTest)
gtest_discover_tests(cordyceps_tests)
