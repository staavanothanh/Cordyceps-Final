#include <gtest/gtest.h>
#include "engine/board.hpp"

namespace cordyceps {

// ── Test 1: EvalWeights struct exists with expected fields ──
TEST(SideWeights, StructHasExpectedFields) {
    EvalWeights w{};
    EXPECT_EQ(w.score, 3);       // default = baseline
    EXPECT_EQ(w.territory, 3);
    EXPECT_EQ(w.corners, 8);
    EXPECT_EQ(w.edges, 2);
    EXPECT_EQ(w.live_adj, 3);
    EXPECT_EQ(w.recapture, 0);
    EXPECT_EQ(w.vulnerability, 0);
}

// ── Test 2: Baseline constant equals evaluate() defaults ──
TEST(SideWeights, BaselineEqualsEvaluateDefaults) {
    // evaluate() with no weights uses score*3 + territory*3 + corners*8 + edges*2 + live_adj*3
    EXPECT_EQ(EvalWeights::baseline().score, 3);
    EXPECT_EQ(EvalWeights::baseline().territory, 3);
    EXPECT_EQ(EvalWeights::baseline().corners, 8);
    EXPECT_EQ(EvalWeights::baseline().edges, 2);
    EXPECT_EQ(EvalWeights::baseline().live_adj, 3);
    EXPECT_EQ(EvalWeights::baseline().recapture, 0);
    EXPECT_EQ(EvalWeights::baseline().vulnerability, 0);
}

// ── Test 3: evaluate() overload with nullptr returns baseline ──
TEST(SideWeights, EvaluateNullptrReturnsBaseline) {
    Board board{};
    board.current_player = k_player_us;
    board.my_score = 50;
    board.opp_score = 20;
    board.eval_cache.my_territory = 10;
    board.eval_cache.opp_territory = 5;
    board.eval_cache.my_corners = 2;
    board.eval_cache.opp_corners = 1;
    board.eval_cache.my_edges = 4;
    board.eval_cache.opp_edges = 2;
    board.eval_cache.live_adj_my = 3;
    board.eval_cache.live_adj_opp = 1;
    board.eval_cache.connectivity_my = 8;
    board.eval_cache.connectivity_opp = 4;

    int v1 = evaluate(board, k_player_us);
    int v2 = evaluate(board, k_player_us, nullptr);

    EXPECT_EQ(v1, v2);
}

// ── Test 4: evaluate() overload with custom weights changes output ──
TEST(SideWeights, EvaluateCustomWeights) {
    Board board{};
    board.current_player = k_player_us;
    board.my_score = 10;
    board.opp_score = 5;
    board.eval_cache.my_territory = 4;
    board.eval_cache.opp_territory = 2;

    // Baseline: 5*3 + 2*3 = 15+6 = 21
    int baseline = evaluate(board, k_player_us, nullptr);

    // Custom: score*10 + territory*0 = 5*10 + 2*0 = 50
    EvalWeights custom{10, 0, 0, 0, 0, 0, 0};
    int custom_val = evaluate(board, k_player_us, &custom);

    EXPECT_EQ(baseline, 21);
    EXPECT_EQ(custom_val, 50);
}

// ── Test 5: evaluate() perspective flip works with custom weights ──
TEST(SideWeights, EvaluatePerspectiveFlipWithCustomWeights) {
    Board board{};
    board.current_player = k_player_opp;
    board.my_score = 10;
    board.opp_score = 20;
    board.eval_cache.my_territory = 4;
    board.eval_cache.opp_territory = 8;
    board.eval_cache.my_corners = 1;
    board.eval_cache.opp_corners = 2;
    board.eval_cache.my_edges = 3;
    board.eval_cache.opp_edges = 6;
    board.eval_cache.live_adj_my = 5;
    board.eval_cache.live_adj_opp = 2;
    board.eval_cache.connectivity_my = 10;
    board.eval_cache.connectivity_opp = 5;

    EvalWeights w{3, 3, 8, 2, 3, 1, -2};

    int us = evaluate(board, k_player_us, &w);
    int opp = evaluate(board, k_player_opp, &w);

    EXPECT_EQ(us, -opp);  // perspective flip must negate
}

} // namespace cordyceps
