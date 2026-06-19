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

// ── Test 6: load_weights_from_file parses format correctly ──
TEST(SideWeights, LoadWeightsFromFile) {
    // Create a temp config file
    FILE* f = std::fopen("test_weights.cfg", "w");
    ASSERT_NE(f, nullptr);
    std::fprintf(f, "FIRST=5 10 15 3 4 2 1\n");
    std::fprintf(f, "SECOND=10 5 8 4 6 3 -2\n");
    std::fclose(f);

    EvalWeights fw{3,3,8,2,3,0,0}, sw{3,3,8,2,3,0,0};
    bool ok = load_weights_from_file("test_weights.cfg", fw, sw);
    EXPECT_TRUE(ok);
    EXPECT_EQ(fw.score, 5);
    EXPECT_EQ(fw.territory, 10);
    EXPECT_EQ(fw.corners, 15);
    EXPECT_EQ(fw.edges, 3);
    EXPECT_EQ(fw.live_adj, 4);
    EXPECT_EQ(fw.recapture, 2);
    EXPECT_EQ(fw.vulnerability, 1);
    EXPECT_EQ(sw.score, 10);
    EXPECT_EQ(sw.territory, 5);
    EXPECT_EQ(sw.vulnerability, -2);

    std::remove("test_weights.cfg");
}

// ── Test 7: deploy_side_weights affects evaluate() ──
TEST(SideWeights, DeploySideWeightsAffectsEval) {
    // Create a temp config
    FILE* f = std::fopen("test_deploy.cfg", "w");
    ASSERT_NE(f, nullptr);
    std::fprintf(f, "FIRST=10 0 0 0 0 0 0\n");  // score*10 only
    std::fprintf(f, "SECOND=0 10 0 0 0 0 0\n");  // territory*10 only
    std::fclose(f);

    EvalWeights fw{3,3,8,2,3,0,0}, sw{3,3,8,2,3,0,0};
    ASSERT_TRUE(load_weights_from_file("test_deploy.cfg", fw, sw));

    // Deploy as FIRST
    deploy_side_weights(true, fw, sw);
    Board board;
    board.current_player = k_player_us;
    board.my_score = 10;
    board.opp_score = 5;
    board.eval_cache.my_territory = 4;
    board.eval_cache.opp_territory = 2;
    // FIRST: score*10 = 5*10 = 50
    int first_eval = evaluate(board, k_player_us);
    EXPECT_EQ(first_eval, 5 * 10);

    // Deploy as SECOND
    deploy_side_weights(false, fw, sw);
    // SECOND: territory*10 = (4-2)*10 = 20
    int second_eval = evaluate(board, k_player_us);
    EXPECT_EQ(second_eval, (4 - 2) * 10);

    clear_tune_weights();
    std::remove("test_deploy.cfg");
}

// ── Test 8: Missing file returns false, baseline unchanged ──
TEST(SideWeights, MissingFileReturnsFalse) {
    EvalWeights fw{3,3,8,2,3,0,0}, sw{3,3,8,2,3,0,0};
    EvalWeights fw_before = fw, sw_before = sw;
    bool ok = load_weights_from_file("nonexistent.cfg", fw, sw);
    EXPECT_FALSE(ok);
    // Weights unchanged
    EXPECT_EQ(fw.score, fw_before.score);
    EXPECT_EQ(sw.score, sw_before.score);
}

} // namespace cordyceps
