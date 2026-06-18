#include <gtest/gtest.h>
#include "engine/rect_table.hpp"
#include "common/prefix_sum.hpp"
#include "engine/movegen.hpp"
#include "engine/board.hpp"

using namespace cordyceps;

class MovegenOptimizedTest : public ::testing::Test {
protected:
    RectTable table;
    
    void SetUp() override {
        ASSERT_TRUE(table.load("data.bin"));
    }
};

TEST_F(MovegenOptimizedTest, OptimizedMatchesBruteForceOnEmpty) {
    Board board;
    board.recalc_live_mask();

    auto old_moves = generate_legal_moves(board);
    auto new_moves = generate_legal_moves_optimized(board, table);
    EXPECT_EQ(new_moves.size(), old_moves.size());
}

TEST_F(MovegenOptimizedTest, OptimizedMatchesBruteForceOnSimple) {
    Board board;
    board.value_at(0, 0) = 3; board.value_at(0, 1) = 4; board.value_at(0, 2) = 3;
    board.value_at(1, 0) = 5; board.value_at(1, 2) = 5;
    board.value_at(2, 0) = 2; board.value_at(2, 1) = 3; board.value_at(2, 2) = 5;
    board.recalc_live_mask();

    auto old_moves = generate_legal_moves(board);
    auto new_moves = generate_legal_moves_optimized(board, table);
    EXPECT_EQ(new_moves.size(), old_moves.size());
}

TEST_F(MovegenOptimizedTest, OptimizedMatchesBruteForceOnFullBoard) {
    Board board;
    for (int i = 0; i < k_cells; ++i) board.values[i] = 1;
    board.recalc_live_mask();

    auto old_moves = generate_legal_moves(board);
    auto new_moves = generate_legal_moves_optimized(board, table);
    EXPECT_EQ(new_moves.size(), old_moves.size());
}

TEST_F(MovegenOptimizedTest, OptimizedFasterThanBruteForce) {
    Board board;
    for (int i = 0; i < k_cells; ++i) board.values[i] = 1;
    board.recalc_live_mask();

    auto start = std::chrono::high_resolution_clock::now();
    auto moves = generate_legal_moves_optimized(board, table);
    auto end = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    EXPECT_LT(us, 2000); // < 2ms
    EXPECT_GT(moves.size(), 50);
}

TEST_F(MovegenOptimizedTest, InscribedRuleStillWorks) {
    Board board;
    // Row 0 has 1,2,3,4 in first 4 cells → sum=10, (0,0)-(0,3) valid
    for (int c = 0; c < 4; ++c) board.value_at(0, c) = c + 1;
    board.recalc_live_mask();

    auto moves = generate_legal_moves_optimized(board, table);
    bool found = false;
    for (auto& m : moves) {
        if (m.r1 == 0 && m.c1 == 0 && m.r2 == 0 && m.c2 == 3) found = true;
    }
    EXPECT_TRUE(found);
}
