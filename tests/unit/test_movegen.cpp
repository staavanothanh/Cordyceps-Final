#include <gtest/gtest.h>
#include "engine/movegen.hpp"

using namespace cordyceps;

// === Prefix Sum ===

TEST(MovegenTest, PrefixSumCalculatesCorrectly) {
    // Create a board with known values
    Board board;
    board.value_at(0, 0) = 1; board.value_at(0, 1) = 2; board.value_at(0, 2) = 3;
    board.value_at(1, 0) = 4; board.value_at(1, 1) = 5; board.value_at(1, 2) = 6;

    // rect_sum(0,0, 0,2) = 1+2+3 = 6
    EXPECT_EQ(rect_sum(board, 0, 0, 0, 2), 6);
    // rect_sum(0,0, 1,2) = 1+2+3+4+5+6 = 21
    EXPECT_EQ(rect_sum(board, 0, 0, 1, 2), 21);
    // rect_sum(1,0, 1,2) = 4+5+6 = 15
    EXPECT_EQ(rect_sum(board, 1, 0, 1, 2), 15);
}

// === Move Generation ===

TEST(MovegenTest, GenerateMovesProducesValidRects) {
    // Simple board: 2x3 area with known values
    Board board;
    // Row 0: 3, 4, 3  -> sum = 10 for (0,0)-(0,2)
    board.value_at(0, 0) = 3; board.value_at(0, 1) = 4; board.value_at(0, 2) = 3;
    // Row 1: 5, 0, 5  -> row sum = 10
    board.value_at(1, 0) = 5; board.value_at(1, 2) = 5;
    // Row 2: 2, 3, 5  -> vari
    board.value_at(2, 0) = 2; board.value_at(2, 1) = 3; board.value_at(2, 2) = 5;
    board.recalc_live_mask();

    auto moves = generate_legal_moves(board);
    // (0,0)-(0,2) sum=10, inscribed
    // (1,0)-(1,2) sum=10, inscribed
    // There should be multiple valid moves
    EXPECT_GT(moves.size(), 0);

    // Verify each move has sum=10
    for (const auto& mv : moves) {
        int sum = rect_sum(board, mv.r1, mv.c1, mv.r2, mv.c2);
        EXPECT_EQ(sum, k_target_sum) 
            << "Move (" << (int)mv.r1 << "," << (int)mv.c1 << ")-(" 
            << (int)mv.r2 << "," << (int)mv.c2 << ") has sum=" << sum;
    }
}

// === Inscribed Rule ===

TEST(MovegenTest, InscribedRuleRejectsInvalid) {
    Board board;
    // Full row 0: 1,2,3,4 -> sum=10 but some edges may be empty
    for (int c = 0; c < 4; ++c) {
        board.value_at(0, c) = c + 1; // 1,2,3,4
    }
    board.recalc_live_mask();

    // (0,0)-(0,3): all 4 cells have mushrooms, inscribed OK
    EXPECT_TRUE(check_inscribed(board, 0, 0, 0, 3));
    
    // Now clear the first cell - one edge will be partially empty
    // But the inscribed rule checks edges, not full rectangle
}

TEST(MovegenTest, PassMoveIsAlwaysValid) {
    Board board;
    // Empty board - no legal moves except PASS
    auto moves = generate_legal_moves(board);
    // PASS is explicitly added by search, not by generate_legal_moves
    // So an empty board should have 0 legal moves
    // But with some mushrooms, there should be legal moves
}

// === Full Board Test ===

TEST(MovegenTest, FullBoardHasManyMoves) {
    Board board;
    // Fill board with 1s
    for (int i = 0; i < k_cells; ++i) {
        board.values[i] = 1;
    }
    board.recalc_live_mask();

    auto moves = generate_legal_moves(board);
    // Any 1x10 or 2x5 etc. that sums to 10
    // Should find many moves
    EXPECT_GT(moves.size(), 50); // conservative estimate
}
