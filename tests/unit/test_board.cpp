#include <gtest/gtest.h>
#include "engine/board.hpp"

using namespace cordyceps;

// Helper: create a simple board with known values
Board make_simple_board() {
    Board board;
    // Fill first row with values 1-9 and some zeros
    board.values[0] = 1;  board.values[1] = 2;  board.values[2] = 3;
    board.values[3] = 4;  board.values[10] = 5; board.values[11] = 6;
    board.values[20] = 7;  board.values[21] = 8; board.values[22] = 9;
    board.recalc_live_mask();
    return board;
}

// === Construction ===

TEST(BoardTest, NewBoardHasZeroScore) {
    Board board;
    EXPECT_EQ(board.my_score, 0);
    EXPECT_EQ(board.opp_score, 0);
}

TEST(BoardTest, NewBoardHasPlayerUnset) {
    Board board;
    EXPECT_EQ(board.current_player, 0);
}

// === Live Mask ===

TEST(BoardTest, LiveMaskMatchesValues) {
    auto board = make_simple_board();
    EXPECT_EQ(board.live_mask.popcount(), 9);
    EXPECT_EQ(board.live_count, 9);
}

// === Apply Move (basic) ===

TEST(BoardTest, ApplyMoveChangesPlayer) {
    auto board = make_simple_board();
    board.current_player = k_player_us;
    // Move to take cells 0 and 1 (values sum=3, but just test mechanics)
    Move mv{0, 0, 0, 1};
    auto undo = board.apply_move(mv);
    // Player should alternate
    EXPECT_EQ(board.current_player, k_player_opp);
}

TEST(BoardTest, ApplyMoveTracksPasses) {
    Board board;
    board.current_player = k_player_us;
    Move pass = k_pass_move;
    auto undo = board.apply_move(pass);
    EXPECT_EQ(board.consecutive_passes, 1);
    EXPECT_EQ(board.current_player, k_player_opp);
    
    board.apply_move(pass);
    EXPECT_EQ(board.consecutive_passes, 2);
}

TEST(BoardTest, ApplyMoveResetsPassesOnRealMove) {
    Board board;
    board.current_player = k_player_us;
    board.consecutive_passes = 1;
    auto undo = board.apply_move(k_pass_move);
    EXPECT_EQ(board.consecutive_passes, 2);
    // Now a real move resets
    board.apply_move({0, 1, 0, 1});
    EXPECT_EQ(board.consecutive_passes, 0);
}

// === Undo Move ===

TEST(BoardTest, UndoRestoresBoard) {
    auto board = make_simple_board();
    board.current_player = k_player_us;
    board.my_score = 0;
    
    auto saved = board; // copy
    
    Move mv{0, 0, 0, 1};
    auto undo = board.apply_move(mv);
    
    // Board should differ
    EXPECT_NE(board.my_score, saved.my_score);
    
    board.unmake_move(undo);
    
    // Should be identical to saved
    EXPECT_EQ(board.my_score, saved.my_score);
    EXPECT_EQ(board.opp_score, saved.opp_score);
    EXPECT_EQ(board.current_player, saved.current_player);
    EXPECT_EQ(board.live_count, saved.live_count);
    EXPECT_EQ(board.consecutive_passes, saved.consecutive_passes);
}

// === Terminal Check ===

TEST(BoardTest, IsTerminalAfterTwoPasses) {
    Board board;
    board.current_player = k_player_us;
    board.consecutive_passes = 2;
    EXPECT_TRUE(board.is_terminal());
}

TEST(BoardTest, IsNotTerminalInitially) {
    Board board;
    EXPECT_FALSE(board.is_terminal());
}

// === Side-Agnostic Score ===

TEST(BoardTest, ScoreFromPlayerPerspective) {
    Board board;
    board.my_score = 10;
    board.opp_score = 3;
    EXPECT_EQ(board.score_from_perspective(k_player_us), 7);
    EXPECT_EQ(board.score_from_perspective(k_player_opp), -7);
}
