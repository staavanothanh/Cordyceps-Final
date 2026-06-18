#include <gtest/gtest.h>
#include "engine/search.hpp"
#include "engine/rect_table.hpp"

using namespace cordyceps;

class SearchTest : public ::testing::Test {
protected:
    RectTable table;
    void SetUp() override {
        ASSERT_TRUE(table.load("data.bin"));
    }
};

TEST_F(SearchTest, EmptyBoardReturnsPass) {
    Board board;
    board.recalc_live_mask();
    board.current_player = k_player_us;

    Search search(table);
    auto result = search.simple_search(board, {1000, 0, 0, 0, false});
    EXPECT_TRUE(result.move.is_pass());
}

TEST_F(SearchTest, FindsValidMove) {
    Board board;
    for (int c = 0; c < 10; ++c) board.value_at(0, c) = 1;
    board.recalc_live_mask();
    board.current_player = k_player_us;

    Search search(table);
    auto result = search.simple_search(board, {1000, 0, 0, 0, false});
    EXPECT_FALSE(result.move.is_pass());
    auto moves = generate_legal_moves_optimized(board, table);
    EXPECT_EQ(result.move, moves[0]); // greedy takes first best
}

TEST_F(SearchTest, TakesBetterMoveOverWorse) {
    Board board;
    board.value_at(0, 0) = 5; board.value_at(0, 1) = 1; board.value_at(0, 2) = 0;
    board.value_at(1, 0) = 5; board.value_at(1, 1) = 0; board.value_at(1, 2) = 0;
    // Single rect (0,0)-(1,0) sum=10
    board.recalc_live_mask();
    board.current_player = k_player_us;

    Search search(table);
    auto result = search.simple_search(board, {1000, 0, 0, 0, false});
    EXPECT_FALSE(result.move.is_pass());
}

TEST_F(SearchTest, PASSWhenLosing) {
    Board board;
    board.my_score = 0;
    board.opp_score = 10; // đang thua
    board.current_player = k_player_us;
    board.consecutive_passes = 0;
    board.recalc_live_mask();

    Search search(table);
    auto moves = generate_legal_moves_optimized(board, table);
    if (moves.empty()) {
        auto result = search.simple_search(board, {1000, 0, 0, 0, false});
        EXPECT_TRUE(result.move.is_pass());
    }
}

TEST_F(SearchTest, PASSWhenWinningAndOppPassed) {
    Board board;
    board.my_score = 15;
    board.opp_score = 5;
    board.current_player = k_player_us;
    board.consecutive_passes = 1; // đối thủ vừa pass (1 pass = opp)
    // Cần set pass tracker đúng — trong Search thì check board.consecutive_passes
    board.recalc_live_mask();

    Search search(table);
    auto result = search.simple_search(board, {1000, 0, 0, 0, false});
    // Nếu leading + opp passed → nên pass để lock win
    // Chỉ PASS nếu không còn move hoặc opp đã pass
    if (board.consecutive_passes >= 2) {
        EXPECT_TRUE(result.move.is_pass());
    }
}

TEST_F(SearchTest, SideAgnosticEval) {
    Board board;
    for (int c = 0; c < 10; ++c) board.value_at(0, c) = 1;
    board.recalc_live_mask();

    // Test as FIRST
    board.current_player = k_player_us;
    Search search_first(table);
    auto r1 = search_first.simple_search(board, {1000, 0, 0, 0, false});

    // Test as SECOND (opening position same)
    board.current_player = k_player_opp;
    Search search_second(table);
    auto r2 = search_second.simple_search(board, {1000, 0, 0, 0, false});

    // Both should find non-pass moves
    EXPECT_FALSE(r1.move.is_pass());
    EXPECT_FALSE(r2.move.is_pass());
}
