#include <gtest/gtest.h>
#include "engine/search.hpp"
#include "engine/rect_table.hpp"
#include "engine/zobrist.hpp"
#include <iostream>
#include <chrono>
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

Board make_board(int seed) {
    Board board;
    int target_live = 60 + (seed * 7) % 50;
    for (int i = 0; i < target_live; ++i) {
        int idx = (i * 37 + seed * 13) % k_cells;
        int val = 1 + ((i + seed) % 9);
        board.values[idx] = static_cast<std::int8_t>(val);
    }
    board.current_player = (seed % 2 == 0) ? k_player_us : k_player_opp;
    board.recalc_live_mask();
    return board;
}

TEST_F(SearchBenchmarkTest, CrashingCase) {
    // Sample 1 is the one that crashes
    Board board = make_board(1);
    std::fprintf(stderr, "live=%d player=%d\n", board.live_count, board.current_player);

    Search search(table, zobrist);
    auto result = search.iterative_deepening(board, 50, {});
    std::fprintf(stderr, "depth=%d eval=%d nodes=%lld\n", result.max_depth, result.eval, result.nodes);
    SUCCEED();
}

TEST_F(SearchBenchmarkTest, SimpleWithOpponentPlayer) {
    Board board;
    for (int c = 0; c < 10; ++c) board.value_at(0, c) = 1;
    board.current_player = k_player_opp;  // opponent's turn
    board.recalc_live_mask();

    Search search(table, zobrist);
    auto result = search.iterative_deepening(board, 100, {});
    std::fprintf(stderr, "depth=%d eval=%d\n", result.max_depth, result.eval);
    EXPECT_GE(result.max_depth, 1);
}

TEST_F(SearchBenchmarkTest, TT_HitRate_Depth4Plus) {
    // Run benchmark on sample 0 separately first, with fresh Search each time
    SearchBenchmark bm{};
    bm.samples = 10;
    
    for (int s = 0; s < 10; ++s) {
        Board board = make_board(s);
        Search search(table, zobrist);
        
        auto start = std::chrono::steady_clock::now();
        auto result = search.iterative_deepening(board, 200, {});
        auto end = std::chrono::steady_clock::now();
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::fprintf(stderr, "  s=%d depth=%d hit_rate=%.1f%% nodes=%lld ms=%lld\n", 
            s, result.max_depth, 
            result.tt_probes > 0 ? (double)result.tt_hits/result.tt_probes*100 : 0,
            result.nodes, elapsed);
        
        bm.avg_depth += result.max_depth;
        bm.avg_nodes += result.nodes;
        bm.avg_ms += elapsed;
        if (result.tt_probes > 0) 
            bm.avg_hit_rate += (double)result.tt_hits / result.tt_probes * 100.0;
    }
    
    bm.avg_depth /= 10; bm.avg_nodes /= 10; bm.avg_ms /= 10; bm.avg_hit_rate /= 10;
    
    std::cout << "\n\n[BENCHMARK] " << bm.samples << " samples, 200ms budget\n";
    std::cout << "  avg_depth:    " << bm.avg_depth << "\n";
    std::cout << "  avg_hit_rate: " << bm.avg_hit_rate << "%\n";
    std::cout << "  avg_nodes:    " << bm.avg_nodes << "\n";
    std::cout << "  avg_ms:       " << bm.avg_ms << "ms\n";

    if (bm.avg_depth >= 4) {
        EXPECT_GT(bm.avg_hit_rate, 30.0) << "TT hit rate should be > 30% at depth 4+";
    }
}

TEST_F(SearchBenchmarkTest, Depth6_Under500ms) {
    SearchBenchmark bm{};
    bm.samples = 5;
    
    for (int s = 0; s < 5; ++s) {
        Board board = make_board(s);
        Search search(table, zobrist);
        
        auto start = std::chrono::steady_clock::now();
        auto result = search.iterative_deepening(board, 500, {});
        auto end = std::chrono::steady_clock::now();
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::fprintf(stderr, "  s=%d depth=%d\n", s, result.max_depth);
        
        bm.avg_depth += result.max_depth;
        bm.avg_nodes += result.nodes;
        bm.avg_ms += elapsed;
        if (result.tt_probes > 0)
            bm.avg_hit_rate += (double)result.tt_hits / result.tt_probes * 100.0;
    }
    
    bm.avg_depth /= 5; bm.avg_nodes /= 5; bm.avg_ms /= 5; bm.avg_hit_rate /= 5;
    
    std::cout << "\n\n[BENCHMARK] " << bm.samples << " samples, 500ms budget\n";
    std::cout << "  avg_depth:    " << bm.avg_depth << "\n";
    std::cout << "  avg_hit_rate: " << bm.avg_hit_rate << "%\n";
    std::cout << "  avg_nodes:    " << bm.avg_nodes << "\n";
    std::cout << "  avg_ms:       " << bm.avg_ms << "ms\n";

    EXPECT_GE(bm.avg_depth, 6.0) << "Should reach depth 6+ in 500ms";
}
