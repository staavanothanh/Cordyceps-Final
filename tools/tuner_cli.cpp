/**
 * tuner_cli.cpp — In-process game simulator for eval weight tuning.
 *
 * Usage: tuner_cli --weights w0 w1 w2 w3 w4 w5 w6 --games N --seed S --time MS
 *
 * Plays N games in-process (no subprocess, no protocol I/O).
 * Each board generates 2 games (swap sides).
 * Opponent always uses default compile-time weights.
 *
 * Output (single line to stdout): margin wins draws losses our_avg opp_avg score_diff elapsed_ms
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#include "common/types.hpp"
#include "common/bitboard.hpp"
#include "engine/board.hpp"
#include "engine/rect_table.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"

using namespace cordyceps;

static uint64_t splitmix64(uint64_t x) noexcept {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

static Board generate_board(uint64_t seed) noexcept {
    Board b;
    std::mt19937_64 rng(seed);
    for (int i = 0; i < k_cells; ++i) {
        int val = 1 + (static_cast<int>(rng()) % 9);
        b.values[i] = static_cast<std::int8_t>(val);
    }
    b.live_count = k_cells;
    b.current_player = k_player_us;
    b.recalc_live_mask();
    return b;
}

struct GameResult {
    int candidate_score;
    int opponent_score;
    bool completed;
};

static GameResult play_game(const RectTable& table, const Zobrist& zobrist,
                            Board board, int time_ms,
                            const int* candidate_weights,
                            bool candidate_is_first) noexcept {
    Search searcher(table, zobrist);
    SideConfig side;

    int candidate_score = 0;
    int opponent_score = 0;
    bool is_candidate_turn = candidate_is_first;

    for (int turn = 0; turn < 200; ++turn) {
        if (board.is_terminal()) break;

        if (is_candidate_turn && candidate_weights) {
            set_tune_weights(candidate_weights[0], candidate_weights[1],
                             candidate_weights[2], candidate_weights[3],
                             candidate_weights[4], candidate_weights[5],
                             candidate_weights[6]);
        } else {
            clear_tune_weights();
        }

        SearchResult result = searcher.iterative_deepening(board, time_ms, side);
        Move best = result.move;

        if (best.is_pass() && board.consecutive_passes >= 1) break;

        if (!best.is_pass()) {
            int gained = 0;
            for (int r = best.r1; r <= best.r2; ++r)
                for (int c = best.c1; c <= best.c2; ++c)
                    if (board.value_at(r, c) > 0) gained++;

            if (is_candidate_turn)
                candidate_score += gained;
            else
                opponent_score += gained;
        }

        // Apply move to board for next turn
        static_cast<void>(board.apply_move(best));
        is_candidate_turn = !is_candidate_turn;
    }

    // Fallback: count any remaining live cells that couldn't be claimed
    if (!board.is_terminal()) {
        for (const auto& v : board.values) {
            if (v == 0) continue; // already eaten
            // Score for whoever would get it — doesn't really matter for tuning,
            // but be conservative: don't assign unplayed cells to either side.
        }
    }

    return GameResult{candidate_score, opponent_score, true};
}

static void evaluate_weights(const int* weights, int num_games,
                             uint64_t base_seed, int time_ms) noexcept {
    RectTable table;
    if (!table.load("data.bin")) {
        std::fprintf(stderr, "ERROR: Cannot load data.bin\n");
        return;
    }
    Zobrist zobrist;

    int num_boards = std::max(1, num_games / 2);
    int wins = 0, draws = 0, losses = 0;
    long long our_total = 0, opp_total = 0;
    int completed = 0;

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for (int b = 0; b < num_boards; ++b) {
        uint64_t seed = splitmix64(base_seed + b);
        Board board = generate_board(seed);

        // Candidate FIRST
        auto g1 = play_game(table, zobrist, board, time_ms, weights, true);
        if (g1.completed) {
            completed++;
            our_total += g1.candidate_score;
            opp_total += g1.opponent_score;
            if (g1.candidate_score > g1.opponent_score) wins++;
            else if (g1.candidate_score == g1.opponent_score) draws++;
            else losses++;
        }

        // Candidate SECOND (same board, swap)
        Board board2 = generate_board(seed);
        auto g2 = play_game(table, zobrist, board2, time_ms, weights, false);
        if (g2.completed) {
            completed++;
            our_total += g2.candidate_score;
            opp_total += g2.opponent_score;
            if (g2.candidate_score > g2.opponent_score) wins++;
            else if (g2.candidate_score == g2.opponent_score) draws++;
            else losses++;
        }
    }
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    if (completed == 0) {
        std::printf("-999.000 0 0 0 0.0 0.0 0 -1");
        return;
    }

    double our_avg = static_cast<double>(our_total) / completed;
    double opp_avg = static_cast<double>(opp_total) / completed;
    double margin = our_avg - opp_avg;
    int score_diff = static_cast<int>(our_total - opp_total);

    // One line for Python to parse
    std::printf("%.4f %d %d %d %.1f %.1f %d %lld\n",
                margin, wins, draws, losses,
                our_avg, opp_avg, score_diff,
                static_cast<long long>(elapsed_ms));

    // Also print to stderr for estimation purposes
    std::fprintf(stderr, "  [tuner] %d games in %lldms (%lldms/game)\n",
                 completed, static_cast<long long>(elapsed_ms),
                 completed > 0 ? static_cast<long long>(elapsed_ms) / completed : 0);
}

int main(int argc, char* argv[]) {
    int weights[7] = {3, 3, 8, 2, 3, 0, 0};
    int num_games = 20;
    uint64_t seed = 42;
    int time_ms = 500;
    bool has_weights = false;
    (void)has_weights;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--weights") == 0 && i + 7 < argc) {
            for (int j = 0; j < 7; ++j)
                weights[j] = std::atoi(argv[i + 1 + j]);
            i += 7;
            has_weights = true;
        } else if (std::strcmp(argv[i], "--games") == 0 && i + 1 < argc) {
            num_games = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            time_ms = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::fprintf(stderr, "Usage: tuner_cli --weights w0 w1 w2 w3 w4 w5 w6 --games N --seed S --time MS\n");
            return 0;
        }
    }

    evaluate_weights(weights, num_games, seed, time_ms);
    return 0;
}
