#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>

#include "common/types.hpp"
#include "engine/board.hpp"
#include "engine/search.hpp"
#include "engine/rect_table.hpp"
#include "engine/zobrist.hpp"

using namespace cordyceps;

// ── Test 1: set_tune_weights changes evaluate() output ──

TEST(TunerWeightsTest, SetTuneWeightsChangesEval) {
    // Create a board with some territory
    Board board;
    board.current_player = k_player_us;

    // Manually assign cells: us at (0,0), opponent at (0,1)
    board.values[0] = 1;   // live mushroom
    board.values[1] = 1;   // live mushroom
    board.owners[0] = k_player_us;
    board.owners[1] = k_player_opp;
    board.my_mask.set(0);
    board.opp_mask.set(1);
    board.live_mask.set(0);
    board.live_mask.set(1);
    board.live_count = 2;
    board.my_score = 1;
    board.opp_score = 1;
    board.eval_cache.my_territory = 1;
    board.eval_cache.opp_territory = 1;

    // Default eval (just verify it doesn't crash)
    static_cast<void>(evaluate(board, k_player_us));

    // Set extreme weights: territory*100, everything else 0
    set_tune_weights(0, 100, 0, 0, 0, 0, 0);
    // territory_diff = 1-1 = 0, so territory-heavy = 0*0 + 0*100 = 0? No difference.
    // Actually territory_diff is 0 because both have 1 territory cell.
    // Let's make a board with clear territory advantage.

    Board board2;
    board2.current_player = k_player_us;
    board2.values[0] = 1;
    board2.values[1] = 1;
    board2.owners[0] = k_player_us;
    board2.owners[1] = k_player_us;  // both are ours
    board2.my_mask.set(0);
    board2.my_mask.set(1);
    board2.live_mask.set(0);
    board2.live_mask.set(1);
    board2.live_count = 2;
    board2.my_score = 2;
    board2.eval_cache.my_territory = 2;
    board2.eval_cache.opp_territory = 0;

    // Default eval
    clear_tune_weights();
    int def2 = evaluate(board2, k_player_us);
    EXPECT_GT(def2, 0);  // we have advantage

    // Extreme territory weight: should amplify advantage
    set_tune_weights(0, 100, 0, 0, 0, 0, 0);
    int ter2 = evaluate(board2, k_player_us);
    EXPECT_GT(ter2, def2);  // territory-heavy > default

    // Verify clearing restores default
    clear_tune_weights();
    int def3 = evaluate(board2, k_player_us);
    EXPECT_EQ(def3, def2);
}

// ── Test 2: Self-play with same weights = margin ≈ 0 ──

TEST(TunerWeightsTest, SelfPlayBalanced) {
    RectTable table;
    ASSERT_TRUE(table.load("data.bin"));
    Zobrist zobrist;

    const int baseline[7] = {3, 3, 8, 2, 3, 0, 0};
    set_tune_weights(baseline[0], baseline[1], baseline[2],
                     baseline[3], baseline[4], baseline[5], baseline[6]);

    int total_diff = 0;
    int games = 8;

    for (int g = 0; g < games; ++g) {
        // Generate board with seed
        Board board;
        std::mt19937_64 rng(42 + g);
        for (int i = 0; i < k_cells; ++i)
            board.values[i] = 1 + static_cast<std::int8_t>(rng() % 9);
        board.live_count = k_cells;
        board.current_player = k_player_us;
        board.recalc_live_mask();

        Search searcher(table, zobrist);
        SideConfig side;

        int our_score = 0;
        int opp_score = 0;
        bool our_turn = true;

        for (int turn = 0; turn < 200; ++turn) {
            if (board.is_terminal()) break;

            // Both sides use SAME weights
            set_tune_weights(baseline[0], baseline[1], baseline[2],
                           baseline[3], baseline[4], baseline[5], baseline[6]);

            SearchResult result = searcher.iterative_deepening(board, 200, side);
            Move best = result.move;

            if (best.is_pass() && board.consecutive_passes >= 1) break;

            if (!best.is_pass()) {
                int gained = 0;
                for (int r = best.r1; r <= best.r2; ++r)
                    for (int c = best.c1; c <= best.c2; ++c)
                        if (board.value_at(r, c) > 0) gained++;
                if (our_turn) our_score += gained;
                else opp_score += gained;
            }

            static_cast<void>(board.apply_move(best));
            our_turn = !our_turn;
        }

        total_diff += (our_score - opp_score);
    }

    clear_tune_weights();
    // With same weights for both sides, margin should be near 0 (±5 for noise with 4 games)
    double avg_margin = static_cast<double>(total_diff) / games;
    EXPECT_NEAR(avg_margin, 0.0, 5.0);
}

// ── Test 3: tuner_cli outputs correct format ──

TEST(TunerWeightsTest, TunerCliFormat) {
    // Run tuner_cli as subprocess (from project root, build/tuner_cli.exe)
    std::string cmd = "build\\tuner_cli.exe --weights 3 3 8 2 3 0 0 --games 8 --seed 42 --time 200";
    FILE* pipe = popen(cmd.c_str(), "r");
    ASSERT_NE(pipe, nullptr);

    char buf[256];
    ASSERT_NE(fgets(buf, sizeof(buf), pipe), nullptr);
    pclose(pipe);

    std::string output(buf);

    // Parse: "margin wins draws losses our_avg opp_avg score_diff elapsed_ms"
    float margin;
    int wins, draws, losses;
    float our_avg, opp_avg;
    int score_diff, elapsed;
    int parsed = std::sscanf(output.c_str(), "%f %d %d %d %f %f %d %d",
                             &margin, &wins, &draws, &losses,
                             &our_avg, &opp_avg, &score_diff, &elapsed);

    EXPECT_EQ(parsed, 8);
    EXPECT_NEAR(margin, 0.0f, 6.0);  // self-play margin ≈ 0 (±noise)
    EXPECT_EQ(wins + draws + losses, 8);  // 8 games
    EXPECT_GT(elapsed, 0);  // took some time
}

// ── Test 4: Different weights produce different margins ──

TEST(TunerWeightsTest, DifferentWeightsDifferentMargin) {
    // Run with baseline
    std::string cmd1 = "build\\tuner_cli.exe --weights 3 3 8 2 3 0 0 --games 8 --seed 42 --time 200";
    FILE* pipe1 = popen(cmd1.c_str(), "r");
    ASSERT_NE(pipe1, nullptr);

    char buf1[256];
    ASSERT_NE(fgets(buf1, sizeof(buf1), pipe1), nullptr);
    pclose(pipe1);

    float margin1;
    std::sscanf(buf1, "%f", &margin1);

    // Run with extreme weights
    std::string cmd2 = "build\\tuner_cli.exe --weights 0 0 0 0 0 0 0 --games 8 --seed 42 --time 200";
    FILE* pipe2 = popen(cmd2.c_str(), "r");
    ASSERT_NE(pipe2, nullptr);

    char buf2[256];
    ASSERT_NE(fgets(buf2, sizeof(buf2), pipe2), nullptr);
    pclose(pipe2);

    float margin2;
    std::sscanf(buf2, "%f", &margin2);

    // Extreme weights (all zeros) should give very different margin from baseline
    EXPECT_NE(margin1, margin2);
}
