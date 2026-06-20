#include "engine/board.hpp"
#include <gtest/gtest.h>

namespace cordyceps {

// ── Test 1: count_safe returns 0 for empty board ──
TEST(SafeEvalTest, EmptyBoardZeroSafe) {
    Board board;
    board.current_player = k_player_us;
    // No cells owned → 0 safe
    int safe_us = count_safe(board, k_player_us);
    int safe_opp = count_safe(board, k_player_opp);
    EXPECT_EQ(safe_us, 0);
    EXPECT_EQ(safe_opp, 0);
}

// ── Test 2: Single isolated cell is safe (no opponent neighbors) ──
TEST(SafeEvalTest, SingleCellSafe) {
    Board board;
    board.current_player = k_player_us;
    board.owners[0*17 + 0] = k_player_us;
    board.eval_cache.my_territory = 1;
    board.my_mask.set(0);
    // Cell (0,0) has no opponent neighbors → safe
    int safe_us = count_safe(board, k_player_us);
    EXPECT_EQ(safe_us, 1);
}

// ── Test 3: Cell adjacent to opponent is NOT safe ──
TEST(SafeEvalTest, CellAdjacentToOpponentNotSafe) {
    Board board;
    board.current_player = k_player_us;
    // Our cell at (0,0)
    board.owners[0] = k_player_us;
    board.eval_cache.my_territory = 1;
    board.my_mask.set(0);
    // Opponent cell at (0,1) — adjacent!
    board.owners[1] = k_player_opp;
    board.eval_cache.opp_territory = 1;
    board.opp_mask.set(1);
    // (0,0) is adjacent to opponent → NOT safe
    int safe_us = count_safe(board, k_player_us);
    EXPECT_EQ(safe_us, 0);
}

// ── Test 4: Connected block interior is safe ──
TEST(SafeEvalTest, BlockInteriorSafe) {
    Board board;
    board.current_player = k_player_us;
    // 2×2 block of our cells at (0,0)-(1,1)
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 2; ++c) {
            int idx = r * 17 + c;
            board.owners[idx] = k_player_us;
            board.my_mask.set(idx);
            board.eval_cache.my_territory++;
        }
    // Opponent cell at (1,2) — adjacent to (1,1)
    board.owners[1*17 + 2] = k_player_opp;
    board.opp_mask.set(1*17+2);
    board.eval_cache.opp_territory = 1;

    // Cells (0,0), (0,1), (1,0) have no opp neighbor → safe
    // Cell (1,1) has opp neighbor at (1,2) → not safe
    int safe_us = count_safe(board, k_player_us);
    EXPECT_EQ(safe_us, 3);
}

// ── Test 5: evaluate() includes safe_diff ──
TEST(SafeEvalTest, EvaluateIncludesSafeDiff) {
    Board board;
    board.current_player = k_player_us;
    // Our cell at (0,0) — safe
    board.owners[0] = k_player_us;
    board.my_mask.set(0);
    board.eval_cache.my_territory = 1;
    // Opponent cell at (5,5) — safe
    board.owners[5*17 + 5] = k_player_opp;
    board.opp_mask.set(5*17+5);
    board.eval_cache.opp_territory = 1;

    int score = evaluate(board, k_player_us);
    // With safe_diff=1-1=0, safe contribution = 0
    // score_diff = 0, territory_diff = 0, rest = 0
    EXPECT_EQ(score, 0);
}

} // namespace cordyceps
