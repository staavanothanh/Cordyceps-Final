#include <gtest/gtest.h>
#include "engine/board.hpp"

using namespace cordyceps;

Board make_eval_test_board() {
    Board board;
    // 3x3 area with values
    board.value_at(0, 0) = 3; board.value_at(0, 1) = 2; board.value_at(0, 2) = 1;
    board.value_at(1, 0) = 2; board.value_at(1, 1) = 0; board.value_at(1, 2) = 1;
    board.value_at(2, 0) = 1; board.value_at(2, 1) = 2; board.value_at(2, 2) = 3;
    board.recalc_live_mask();
    return board;
}

TEST(EvalCacheTest, NewBoardHasZeroEval) {
    Board board;
    board.recalc_live_mask();
    int score = evaluate(board, k_player_us);
    EXPECT_EQ(score, 0);
}

TEST(EvalCacheTest, EvalUpdatesOnMove) {
    auto board = make_eval_test_board();
    board.current_player = k_player_us;

    int before = evaluate(board, k_player_us);
    Move mv{0, 0, 0, 1}; // takes cells (0,0)=3, (0,1)=2 → sum=5, not 10
    auto undo = board.apply_move(mv);
    int after = evaluate(board, k_player_us);
    EXPECT_GT(after, before); // taking mushrooms should improve score

    board.unmake_move(undo);
    int restored = evaluate(board, k_player_us);
    EXPECT_EQ(restored, before);
}

TEST(EvalCacheTest, UnmakeRestoresEval) {
    auto board = make_eval_test_board();
    board.current_player = k_player_us;

    int original = evaluate(board, k_player_us);
    Move mv{0, 0, 0, 2}; // sum=6, not 10 but valid for testing
    auto undo = board.apply_move(mv);
    int after_apply = evaluate(board, k_player_us);

    board.unmake_move(undo);
    EXPECT_EQ(evaluate(board, k_player_us), original);
    EXPECT_EQ(board.eval_cache.my_territory, 0);
}

TEST(EvalCacheTest, ScorePerspective) {
    auto board = make_eval_test_board();
    board.current_player = k_player_us;

    Move mv{0, 1, 1, 2}; // (0,1)=2, (0,2)=1, (1,2)=1 → sum=4
    board.apply_move(mv);

    int us_score = evaluate(board, k_player_us);
    int opp_score = evaluate(board, k_player_opp);
    EXPECT_EQ(us_score, -opp_score); // symmetric

    board.unmake_move(board.apply_move(mv)); // restore
}

TEST(EvalCacheTest, TerritoryCount) {
    auto board = make_eval_test_board();
    board.current_player = k_player_us;

    // Take cell (0,0) only
    Move mv{0, 0, 0, 0};
    auto undo = board.apply_move(mv);

    EXPECT_EQ(board.eval_cache.my_territory, 1);
    EXPECT_EQ(board.eval_cache.opp_territory, 0);
}

TEST(EvalCacheTest, CornerTracking) {
    auto board = make_eval_test_board();
    board.current_player = k_player_us;

    Move mv{0, 0, 0, 0}; // cell (0,0) is corner
    auto undo = board.apply_move(mv);

    EXPECT_EQ(board.eval_cache.my_corners, 1);
    EXPECT_EQ(board.eval_cache.opp_corners, 0);
}

TEST(EvalCacheTest, EdgeTracking) {
    auto board = make_eval_test_board();
    board.current_player = k_player_us;

    Move mv{0, 1, 0, 1}; // cell (0,1) is edge (top row)
    auto undo = board.apply_move(mv);

    EXPECT_EQ(board.eval_cache.my_edges, 1);
}

TEST(EvalCacheTest, FullRecalcMatches) {
    auto board = make_eval_test_board();
    board.current_player = k_player_us;

    auto undo = board.apply_move({0, 0, 0, 1}); // sum=5

    int incremental_score = board.eval_cache.my_territory * 10 + board.eval_cache.my_corners * 5;

    // Manual calc: took 2 cells, (0,0) is corner
    EXPECT_EQ(board.eval_cache.my_territory, 2);
    EXPECT_EQ(board.eval_cache.my_corners, 1);
    EXPECT_EQ(board.eval_cache.my_edges, 2);
}
