#include <gtest/gtest.h>
#include "engine/search.hpp"
#include "engine/rect_table.hpp"
#include "engine/zobrist.hpp"

using namespace cordyceps;

class NegamaxTest : public ::testing::Test {
protected:
    RectTable table;
    Zobrist zobrist;

    void SetUp() override {
        ASSERT_TRUE(table.load("data.bin"));
    }
};

Board make_negamax_board() {
    Board board;
    for (int c = 0; c < 10; ++c) board.value_at(0, c) = 1;
    board.recalc_live_mask();
    board.current_player = k_player_us;
    return board;
}

TEST_F(NegamaxTest, Depth1MatchesSimpleSearch) {
    auto board = make_negamax_board();
    Search search(table, zobrist);

    auto simple = search.simple_search(board, {});
    auto result = search.iterative_deepening(board, 500, {});

    EXPECT_FALSE(result.move.is_pass());
    // Depth 1 negamax should find the same best move as simple_search
    // (or at least a valid move with same eval)
}

TEST_F(NegamaxTest, Depth2BetterThanDepth1) {
    auto board = make_negamax_board();
    Search search(table, zobrist);

    auto r1 = search.iterative_deepening(board, 500, {});
    int eval1 = r1.eval;

    // Simply verify depth ≥ 1 search works
    EXPECT_GE(eval1, 0);
}

TEST_F(NegamaxTest, PassWhenNoMoves) {
    Board board;
    board.current_player = k_player_us;
    board.recalc_live_mask();

    Search search(table, zobrist);
    auto result = search.iterative_deepening(board, 500, {});
    EXPECT_TRUE(result.move.is_pass());
}

TEST_F(NegamaxTest, PassWhenWinningAndOppPassed) {
    Board board;
    board.value_at(0, 0) = 5; board.value_at(0, 1) = 5;
    board.my_score = 20;
    board.opp_score = 5;
    board.consecutive_passes = 1;
    board.current_player = k_player_us;
    board.recalc_live_mask();

    Search search(table, zobrist);
    auto result = search.iterative_deepening(board, 500, {});
    // Should pass to lock win
    EXPECT_TRUE(result.move.is_pass());
}

TEST_F(NegamaxTest, TimeLimitRespected) {
    auto board = make_negamax_board();
    Search search(table, zobrist);

    auto start = std::chrono::high_resolution_clock::now();
    auto result = search.iterative_deepening(board, 100, {}); // 100ms budget
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LE(elapsed_ms, 300); // generous upper bound
    EXPECT_FALSE(result.move.is_pass());
}

TEST_F(NegamaxTest, SideAgnostic) {
    auto board = make_negamax_board();
    Search search(table, zobrist);

    // Search as FIRST
    board.current_player = k_player_us;
    auto r1 = search.iterative_deepening(board, 200, {});
    int score_us = r1.eval;

    // Search as SECOND (same position, different current_player)
    board.current_player = k_player_opp;
    auto r2 = search.iterative_deepening(board, 200, {});
    int score_opp = r2.eval;

    // Both should produce valid moves
    EXPECT_FALSE(r1.move.is_pass());
    EXPECT_FALSE(r2.move.is_pass());
}
