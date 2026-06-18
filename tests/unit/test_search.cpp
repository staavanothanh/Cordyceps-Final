#include <gtest/gtest.h>
#include "engine/search.hpp"
#include "engine/rect_table.hpp"
#include "engine/zobrist.hpp"

using namespace cordyceps;

class SearchTest : public ::testing::Test {
protected:
    RectTable table;
    Zobrist zobrist;
    void SetUp() override {
        ASSERT_TRUE(table.load("data.bin"));
    }
};

TEST_F(SearchTest, EmptyBoardReturnsPass) {
    Board board;
    board.recalc_live_mask();
    board.current_player = k_player_us;

    Search search(table, zobrist);
    auto result = search.simple_search(board, {1000, 0, 0, 0, false});
    EXPECT_TRUE(result.move.is_pass());
}

TEST_F(SearchTest, FindsValidMove) {
    Board board;
    for (int c = 0; c < 10; ++c) board.value_at(0, c) = 1;
    board.recalc_live_mask();
    board.current_player = k_player_us;

    Search search(table, zobrist);
    auto result = search.simple_search(board, {1000, 0, 0, 0, false});
    EXPECT_FALSE(result.move.is_pass());
}

TEST_F(SearchTest, TakesBetterMoveOverWorse) {
    Board board;
    board.value_at(0, 0) = 5; board.value_at(0, 1) = 1; board.value_at(0, 2) = 0;
    board.value_at(1, 0) = 5; board.value_at(1, 1) = 0; board.value_at(1, 2) = 0;
    board.recalc_live_mask();
    board.current_player = k_player_us;

    Search search(table, zobrist);
    auto result = search.simple_search(board, {1000, 0, 0, 0, false});
    EXPECT_FALSE(result.move.is_pass());
}

TEST_F(SearchTest, PASSWhenLosing) {
    Board board;
    board.my_score = 0;
    board.opp_score = 10;
    board.current_player = k_player_us;
    board.consecutive_passes = 0;
    board.recalc_live_mask();

    Search search(table, zobrist);
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
    board.consecutive_passes = 1;
    board.recalc_live_mask();

    Search search(table, zobrist);
    auto result = search.simple_search(board, {1000, 0, 0, 0, false});
    if (board.consecutive_passes >= 2) {
        EXPECT_TRUE(result.move.is_pass());
    }
}

TEST_F(SearchTest, SideAgnosticEval) {
    Board board;
    for (int c = 0; c < 10; ++c) board.value_at(0, c) = 1;
    board.recalc_live_mask();

    board.current_player = k_player_us;
    Search search_first(table, zobrist);
    auto r1 = search_first.simple_search(board, {1000, 0, 0, 0, false});

    board.current_player = k_player_opp;
    Search search_second(table, zobrist);
    auto r2 = search_second.simple_search(board, {1000, 0, 0, 0, false});

    EXPECT_FALSE(r1.move.is_pass());
    EXPECT_FALSE(r2.move.is_pass());
}
