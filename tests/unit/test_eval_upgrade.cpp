#include <gtest/gtest.h>
#include "engine/board.hpp"
#include "engine/search.hpp"
#include "engine/movegen.hpp"
#include "engine/rect_table.hpp"
#include "engine/zobrist.hpp"
#include "engine/timeman.hpp"
#include "common/types.hpp"

using namespace cordyceps;

// Forward declaration of helper
static RectTable load_test_table();

// ===== Connectivity Tests =====

TEST(ConnectivityTest, AdjacentCellsIncreaseConnectivity) {
    Board board;
    board.current_player = k_player_us;
    board.value_at(0, 0) = 5;
    board.value_at(0, 1) = 5;
    board.recalc_live_mask();

    auto _ = board.apply_move({0, 0, 0, 1});
    (void)_;

    // Two adjacent cells should have connectivity > 0
    EXPECT_GT(board.eval_cache.connectivity_my, 0);
}

TEST(ConnectivityTest, NonAdjacentCellsZeroConnectivity) {
    Board board;
    board.current_player = k_player_us;
    board.value_at(0, 0) = 10;
    board.value_at(2, 2) = 10;
    board.recalc_live_mask();

    // Claim separate non-adjacent cells
    board.apply_move({0, 0, 0, 0});

    EXPECT_EQ(board.eval_cache.connectivity_my, 0);
}

TEST(ConnectivityTest, ThreeHorizontalCellsTwoEdges) {
    Board board;
    board.current_player = k_player_us;
    board.value_at(0, 0) = 3;
    board.value_at(0, 1) = 3;
    board.value_at(0, 2) = 4;
    board.recalc_live_mask();

    board.apply_move({0, 0, 0, 2});
    EXPECT_EQ(board.eval_cache.connectivity_my, 2);
}

TEST(ConnectivityTest, UnmakeRestoresConnectivity) {
    Board board;
    board.current_player = k_player_us;
    board.value_at(0, 0) = 5;
    board.value_at(0, 1) = 5;
    board.recalc_live_mask();

    auto undo = board.apply_move({0, 0, 0, 1});
    int after = board.eval_cache.connectivity_my;
    EXPECT_GT(after, 0);

    board.unmake_move(undo);
    EXPECT_EQ(board.eval_cache.connectivity_my, 0);
}

TEST(ConnectivityTest, VerticalConnectivity) {
    Board board;
    board.current_player = k_player_us;
    board.value_at(0, 0) = 5;
    board.value_at(1, 0) = 5;
    board.recalc_live_mask();

    board.apply_move({0, 0, 1, 0});
    EXPECT_EQ(board.eval_cache.connectivity_my, 1);
}

TEST(ConnectivityTest, OpponentConnectivitySeparate) {
    Board board;
    board.current_player = k_player_us;
    board.value_at(0, 0) = 10;
    board.value_at(5, 5) = 10;
    board.recalc_live_mask();

    board.apply_move({0, 0, 0, 0});
    board.current_player = k_player_opp;
    board.apply_move({5, 5, 5, 5});

    EXPECT_EQ(board.eval_cache.connectivity_my, 0);
    EXPECT_EQ(board.eval_cache.connectivity_opp, 0);
}

// ===== Mobility Test =====

TEST(MobilityTest, CountsLegalMoves) {
    Board board;
    board.current_player = k_player_us;
    board.value_at(0, 0) = 3;
    board.value_at(0, 1) = 3;
    board.value_at(0, 2) = 4;
    board.value_at(1, 0) = 2;
    board.value_at(1, 1) = 2;
    board.value_at(1, 2) = 1;
    board.recalc_live_mask();

    auto moves = generate_legal_moves_optimized(board, load_test_table());
    EXPECT_GT(moves.size(), 0);
}

// ===== Endgame Solver Tests =====

TEST(EndgameTest, SingleMoveLeft) {
    RectTable table = load_test_table();
    Zobrist zobrist;

    Board board;
    board.current_player = k_player_us;
    board.value_at(0, 0) = 10;
    board.recalc_live_mask();

    Search search(table, zobrist);
    auto result = search.iterative_deepening(board, 2000, {});

    EXPECT_EQ(result.move.r1, 0);
    EXPECT_EQ(result.move.c1, 0);
    EXPECT_EQ(result.move.r2, 0);
    EXPECT_EQ(result.move.c2, 0);
}

TEST(EndgameTest, TerminalPositionReturnsPass) {
    RectTable table = load_test_table();
    Zobrist zobrist;

    Board board;
    board.current_player = k_player_us;
    board.consecutive_passes = 2;

    Search search(table, zobrist);
    auto result = search.iterative_deepening(board, 1000, {});

    EXPECT_TRUE(result.move.is_pass());
}

TEST(EndgameTest, EndgameDetectedAtLowLiveCount) {
    Board board;
    board.value_at(0, 0) = 1;
    board.value_at(0, 1) = 2;
    board.value_at(0, 2) = 3;
    board.value_at(0, 3) = 4;
    board.recalc_live_mask();
    board.live_count = 12;

    auto phase = detect_phase(board);
    EXPECT_EQ(phase, GamePhase::kEndgame);
}

// ===== Futility Pruning Test =====

TEST(FutilityTest, WinningPositionFindsMove) {
    RectTable table = load_test_table();
    Zobrist zobrist;

    Board board;
    board.current_player = k_player_us;
    board.value_at(0, 0) = 5;
    board.value_at(0, 1) = 5;
    board.recalc_live_mask();

    Search search(table, zobrist);
    auto result = search.iterative_deepening(board, 500, {});

    EXPECT_FALSE(result.move.is_pass());
}

// ----- Helper -----

static RectTable load_test_table() {
    RectTable table;
    bool loaded = table.load("data.bin");
    if (!loaded) {
        loaded = table.load("build/data.bin");
    }
    return table;
}
