#include <gtest/gtest.h>
#include "engine/search.hpp"
#include "engine/rect_table.hpp"
#include "engine/zobrist.hpp"
#include <iostream>
#include <cstdio>

using namespace cordyceps;

class SearchBenchmarkTest : public ::testing::Test {
protected:
    RectTable table;
    Zobrist zobrist;

    void SetUp() override {
        ASSERT_TRUE(table.load("data.bin"));
    }
};

TEST_F(SearchBenchmarkTest, TT_HitRate_Depth4Plus) {
    SearchBenchmark bm{};
    bm.samples = 10;

    for (int s = 0; s < 10; ++s) {
        Board board;
        for (int i = 0; i < 60 + (s * 7) % 50; ++i) {
            int idx = (i * 37 + s * 13) % k_cells;
            board.values[idx] = static_cast<std::int8_t>(1 + ((i + s) % 9));
        }
        board.current_player = (s % 2 == 0) ? k_player_us : k_player_opp;
        board.recalc_live_mask();

        Search search(table, zobrist);
        auto start = std::chrono::steady_clock::now();
        auto result = search.iterative_deepening(board, 200, {});
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::fprintf(stderr, "  s=%d depth=%d hit=%.1f%% nodes=%lld ms=%lld\n",
            s, result.max_depth,
            result.tt_probes > 0 ? (double)result.tt_hits / result.tt_probes * 100 : 0,
            result.nodes, elapsed);

        bm.avg_depth += result.max_depth;
        bm.avg_nodes += result.nodes;
        bm.avg_ms += elapsed;
        if (result.tt_probes > 0)
            bm.avg_hit_rate += (double)result.tt_hits / result.tt_probes * 100.0;
    }

    bm.avg_depth /= 10; bm.avg_nodes /= 10; bm.avg_ms /= 10; bm.avg_hit_rate /= 10;

    std::cout << "\n[BENCHMARK 200ms] avg_depth=" << bm.avg_depth
              << " hit_rate=" << bm.avg_hit_rate << "%"
              << " nodes=" << bm.avg_nodes
              << " ms=" << bm.avg_ms << "\n";

    EXPECT_GE(bm.avg_depth, 6) << "Should reach depth 6+ in 200ms";
    if (bm.avg_depth >= 4) {
        EXPECT_GT(bm.avg_hit_rate, 30.0);
    }
}

TEST_F(SearchBenchmarkTest, Depth6_Under500ms) {
    SearchBenchmark bm{};
    bm.samples = 5;

    for (int s = 0; s < 5; ++s) {
        Board board;
        for (int i = 0; i < 60 + (s * 7) % 50; ++i) {
            int idx = (i * 37 + s * 13) % k_cells;
            board.values[idx] = static_cast<std::int8_t>(1 + ((i + s) % 9));
        }
        board.current_player = (s % 2 == 0) ? k_player_us : k_player_opp;
        board.recalc_live_mask();

        Search search(table, zobrist);
        auto start = std::chrono::steady_clock::now();
        auto result = search.iterative_deepening(board, 500, {});
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::fprintf(stderr, "  s=%d depth=%d nodes=%lld ms=%lld\n",
            s, result.max_depth, result.nodes, elapsed);

        bm.avg_depth += result.max_depth;
        bm.avg_nodes += result.nodes;
        bm.avg_ms += elapsed;
        if (result.tt_probes > 0)
            bm.avg_hit_rate += (double)result.tt_hits / result.tt_probes * 100.0;
    }

    bm.avg_depth /= 5; bm.avg_nodes /= 5; bm.avg_ms /= 5; bm.avg_hit_rate /= 5;

    std::cout << "\n[BENCHMARK 500ms] avg_depth=" << bm.avg_depth
              << " hit_rate=" << bm.avg_hit_rate << "%"
              << " nodes=" << bm.avg_nodes
              << " ms=" << bm.avg_ms << "\n";

    EXPECT_GE(bm.avg_depth, 8) << "Should reach depth 8+ in 500ms";
}
