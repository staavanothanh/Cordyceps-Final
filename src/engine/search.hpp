#ifndef CORDYCEPS_ENGINE_SEARCH_HPP
#define CORDYCEPS_ENGINE_SEARCH_HPP

#include <chrono>
#include "common/types.hpp"
#include "engine/board.hpp"
#include "engine/rect_table.hpp"
#include "engine/movegen.hpp"
#include "engine/zobrist.hpp"
#include "engine/tt.hpp"

namespace cordyceps {

struct SearchResult {
    Move move;
    int eval;
    int max_depth{0};
    long long tt_probes{0};
    long long tt_hits{0};
    long long nodes{0};
};

struct SearchBenchmark {
    double avg_depth;
    double avg_hit_rate;
    double avg_nodes;
    double avg_ms;
    int samples;
};

class Search {
public:
    Search(const RectTable& table, const Zobrist& zobrist) noexcept
        : table_(table), zobrist_(zobrist), tt_(18) {} // 2^18 = 256K entries

    [[nodiscard]] SearchResult simple_search(const Board& board, const SideConfig& config) noexcept;
    
    // Iterative deepening with negamax
    [[nodiscard]] SearchResult iterative_deepening(Board& board, int time_ms, const SideConfig& config) noexcept;

    // Benchmark helper: run iterative_deepening on multiple random boards
    [[nodiscard]] static SearchBenchmark benchmark(const RectTable& table, const Zobrist& zobrist, int time_ms, int samples) noexcept;

private:
    const RectTable& table_;
    const Zobrist& zobrist_;
    TranspositionTable tt_;

    // Killer moves: 2 per depth (max depth 64)
    Move killer1_[64]{};
    Move killer2_[64]{};

    // Time management
    std::chrono::steady_clock::time_point start_time_;
    int time_limit_ms_{0};
    int node_count_{0};
    bool timed_out_{false};

    // TT statistics (per search)
    long long tt_probes_{0};
    long long tt_hits_{0};
    int max_depth_reached_{0};

    // History heuristic: tracks which (r1,c1,r2,c2) pairs are successful
    int history_[k_rows][k_cols][k_rows][k_cols]{};

    [[nodiscard]] bool time_check() noexcept;

    // Move ordering score
    [[nodiscard]] int order_score(const Board& board, const Move& mv, int depth) noexcept;

    // Negamax α-β
    int negamax(Board& board, int depth, int alpha, int beta, bool allow_pass) noexcept;

    // Move ordering: sort moves before search loop
    void sort_moves(Board& board, std::vector<Move>& moves, int depth, const Move& tt_move) noexcept;
};

} // namespace cordyceps

#endif // CORDYCEPS_ENGINE_SEARCH_HPP
