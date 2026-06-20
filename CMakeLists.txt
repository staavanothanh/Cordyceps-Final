cmake_minimum_required(VERSION 3.20)
project(cordyceps LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O3 -march=native -static -static-libgcc -static-libstdc++)
endif()

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")

# Source files (for reference)
# Built via add_executable below

# Test files (added incrementally as tests written)
set(UNIT_TESTS
    tests/unit/test_bitboard.cpp
    tests/unit/test_board.cpp
    tests/unit/test_movegen.cpp
    tests/unit/test_movegen_optimized.cpp
    tests/unit/test_prefixsum.cpp
    tests/unit/test_recttable.cpp
    tests/unit/test_eval.cpp
    tests/unit/test_search.cpp
    tests/unit/test_zobrist.cpp
    tests/unit/test_tt.cpp
    tests/unit/test_negamax.cpp
    tests/unit/test_benchmark.cpp
    tests/unit/test_timeman.cpp
    tests/unit/test_time_benchmark.cpp
    tests/unit/test_protocol_init.cpp
    tests/unit/test_protocol_flow.cpp
    tests/unit/test_turn2_crash.cpp
    tests/unit/test_eval_upgrade.cpp
    tests/unit/test_tuner_weights.cpp
tests/unit/test_side_weights.cpp
tests/unit/test_safe_eval.cpp
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
    src/common/prefix_sum.cpp
    src/engine/board.cpp
    src/engine/movegen.cpp
    src/engine/rect_table.cpp
    src/engine/search.cpp
    src/engine/timeman.cpp
    src/engine/zobrist.cpp
    src/engine/tt.cpp
    src/io/protocol.cpp
    src/main.cpp
)
target_include_directories(cordyceps PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Test executable
add_executable(cordyceps_tests ${UNIT_TESTS} 
    src/common/bitboard.cpp
    src/common/prefix_sum.cpp
    src/engine/board.cpp
    src/engine/movegen.cpp
    src/engine/rect_table.cpp
    src/engine/search.cpp
    src/engine/timeman.cpp
    src/engine/zobrist.cpp
    src/engine/tt.cpp
    src/io/protocol.cpp
)
target_include_directories(cordyceps_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cordyceps_tests GTest::gtest GTest::gmock GTest::gtest_main)

# CTest
enable_testing()
include(GoogleTest)
gtest_discover_tests(cordyceps_tests)

# Tuner CLI (weight optimization tool)
add_executable(tuner_cli
    tools/tuner_cli.cpp
    src/common/bitboard.cpp
    src/common/prefix_sum.cpp
    src/engine/board.cpp
    src/engine/movegen.cpp
    src/engine/rect_table.cpp
    src/engine/search.cpp
    src/engine/timeman.cpp
    src/engine/zobrist.cpp
    src/engine/tt.cpp
)
target_include_directories(tuner_cli PRIVATE ${CMAKE_SOURCE_DIR}/src)
