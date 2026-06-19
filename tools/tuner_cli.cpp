/**
 * tuner_cli.cpp — In-process game simulator for eval weight tuning.
 *
 * Usage:
 *   tuner_cli --weights W0 W1 W2 W3 W4 W5 W6 --games N --seed S --time MS
 *   tuner_cli --weights-first FW0..FW6 --weights-second SW0..SW6 --games N --seed S --time MS
 *
 * Single-weight mode: candidate uses same weights for both sides (backward compat).
 * Dual-weight mode: candidate uses FIRST weights when first, SECOND when second.
 * Opponent always uses default compile-time weights.
 *
 * Output: margin wins draws losses our_avg opp_avg score_diff elapsed_ms
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

static void parse_weights(int argc, char* argv[], int& i,
                          int* out, int count) noexcept {
    for (int j = 0; j < count && i + 1 < argc; ++j)
        out[j] = std::atoi(argv[++i]);
}

int main(int argc, char* argv[]) {
    // Support both single (7) and dual (14) weight modes
    int fw[7] = {3, 3, 8, 2, 3, 0, 0};  // FIRST weights
    int sw[7] = {3, 3, 8, 2, 3, 0, 0};  // SECOND weights
    int num_games = 20;
    uint64_t seed = 42;
    int time_ms = 500;
    bool has_fw = false, has_sw = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--weights-first") == 0 && i + 7 < argc) {
            parse_weights(argc, argv, i, fw, 7);
            has_fw = true;
        } else if (std::strcmp(argv[i], "--weights-second") == 0 && i + 7 < argc) {
            parse_weights(argc, argv, i, sw, 7);
            has_sw = true;
        } else if (std::strcmp(argv[i], "--weights") == 0 && i + 7 < argc) {
            parse_weights(argc, argv, i, fw, 7);
            std::memcpy(sw, fw, sizeof(fw));
            has_fw = true;
            has_sw = true;
        } else if (std::strcmp(argv[i], "--games") == 0 && i + 1 < argc) {
            num_games = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            time_ms = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::fprintf(stderr,
                "Usage:\n"
                "  tuner_cli --weights W0..W6 --games N --seed S --time MS\n"
                "  tuner_cli --weights-first FW0..FW6 --weights-second SW0..SW6 --games N --seed S --time MS\n");
            return 0;
        }
    }

    // Load table
    RectTable table;
    if (!table.load("data.bin")) {
        std::fprintf(stderr, "ERROR: Cannot load data.bin\n");
        return 1;
    }
    Zobrist zobrist;

    int num_boards = std::max(1, num_games / 2);
    int wins = 0, draws = 0, losses = 0;
    long long our_total = 0, opp_total = 0;
    int completed = 0;

    // Pre-create searches (one per side, for TT reuse)
    Search search_f(table, zobrist);
    Search search_s(table, zobrist);

    SideConfig side;  // Both searches use same side config (default)

    auto start = std::chrono::steady_clock::now();

    // === Dual-weight mode: candidate uses FW when first, SW when second ===
    if (has_fw && has_sw) {
        for (int b = 0; b < num_boards; ++b) {
            uint64_t board_seed = splitmix64(seed + b);

            // --- Game 1: candidate FIRST (fw) vs baseline SECOND ---
            {
                Board board = generate_board(board_seed);
                Search* searcher = &search_f;
                int candidate_score = 0, opponent_score = 0;
                bool is_candidate_turn = true;

                for (int turn = 0; turn < 200 && !board.is_terminal(); ++turn) {
                    if (is_candidate_turn) {
                        set_tune_weights(fw[0], fw[1], fw[2], fw[3], fw[4], fw[5], fw[6]);
                    } else {
                        clear_tune_weights();
                    }

                    SearchResult result = searcher->iterative_deepening(board, time_ms, side);
                    Move best = result.move;

                    if (best.is_pass() && board.consecutive_passes >= 1) break;

                    if (!best.is_pass()) {
                        int gained = 0;
                        for (int r = best.r1; r <= best.r2; ++r)
                            for (int c = best.c1; c <= best.c2; ++c)
                                if (board.value_at(r, c) > 0) gained++;
                        if (is_candidate_turn) candidate_score += gained;
                        else opponent_score += gained;
                    }

                    static_cast<void>(board.apply_move(best));
                    is_candidate_turn = !is_candidate_turn;
                }

                completed++;
                our_total += candidate_score;
                opp_total += opponent_score;
                if (candidate_score > opponent_score) wins++;
                else if (candidate_score == opponent_score) draws++;
                else losses++;
            }

            // --- Game 2: candidate SECOND (sw) vs baseline FIRST ---
            {
                Board board = generate_board(board_seed);
                Search* searcher = &search_s;
                int candidate_score = 0, opponent_score = 0;
                bool is_candidate_turn = false;

                for (int turn = 0; turn < 200 && !board.is_terminal(); ++turn) {
                    if (is_candidate_turn) {
                        set_tune_weights(sw[0], sw[1], sw[2], sw[3], sw[4], sw[5], sw[6]);
                    } else {
                        clear_tune_weights();
                    }

                    SearchResult result = searcher->iterative_deepening(board, time_ms, side);
                    Move best = result.move;

                    if (best.is_pass() && board.consecutive_passes >= 1) break;

                    if (!best.is_pass()) {
                        int gained = 0;
                        for (int r = best.r1; r <= best.r2; ++r)
                            for (int c = best.c1; c <= best.c2; ++c)
                                if (board.value_at(r, c) > 0) gained++;
                        if (is_candidate_turn) candidate_score += gained;
                        else opponent_score += gained;
                    }

                    static_cast<void>(board.apply_move(best));
                    is_candidate_turn = !is_candidate_turn;
                }

                completed++;
                our_total += candidate_score;
                opp_total += opponent_score;
                if (candidate_score > opponent_score) wins++;
                else if (candidate_score == opponent_score) draws++;
                else losses++;
            }
        }
    }
    // === Single-weight mode: backward compat (candidate uses same weights both sides) ===
    else {
        for (int b = 0; b < num_boards; ++b) {
            uint64_t board_seed = splitmix64(seed + b);

            // Candidate FIRST
            {
                Board board = generate_board(board_seed);
                Search searcher(table, zobrist);
                int candidate_score = 0, opponent_score = 0;
                bool is_candidate_turn = true;

                for (int turn = 0; turn < 200 && !board.is_terminal(); ++turn) {
                    if (is_candidate_turn) {
                        set_tune_weights(fw[0], fw[1], fw[2], fw[3], fw[4], fw[5], fw[6]);
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
                        if (is_candidate_turn) candidate_score += gained;
                        else opponent_score += gained;
                    }

                    static_cast<void>(board.apply_move(best));
                    is_candidate_turn = !is_candidate_turn;
                }

                completed++;
                our_total += candidate_score;
                opp_total += opponent_score;
                if (candidate_score > opponent_score) wins++;
                else if (candidate_score == opponent_score) draws++;
                else losses++;
            }

            // Candidate SECOND
            {
                Board board = generate_board(board_seed);
                Search searcher(table, zobrist);
                int candidate_score = 0, opponent_score = 0;
                bool is_candidate_turn = false;

                for (int turn = 0; turn < 200 && !board.is_terminal(); ++turn) {
                    if (is_candidate_turn) {
                        set_tune_weights(fw[0], fw[1], fw[2], fw[3], fw[4], fw[5], fw[6]);
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
                        if (is_candidate_turn) candidate_score += gained;
                        else opponent_score += gained;
                    }

                    static_cast<void>(board.apply_move(best));
                    is_candidate_turn = !is_candidate_turn;
                }

                completed++;
                our_total += candidate_score;
                opp_total += opponent_score;
                if (candidate_score > opponent_score) wins++;
                else if (candidate_score == opponent_score) draws++;
                else losses++;
            }
        }
    }

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    if (completed == 0) {
        std::printf("-999.000 0 0 0 0.0 0.0 0 -1");
        return 0;
    }

    double our_avg = static_cast<double>(our_total) / completed;
    double opp_avg = static_cast<double>(opp_total) / completed;
    double margin = our_avg - opp_avg;
    int score_diff = static_cast<int>(our_total - opp_total);

    std::printf("%.4f %d %d %d %.1f %.1f %d %lld\n",
                margin, wins, draws, losses,
                our_avg, opp_avg, score_diff,
                static_cast<long long>(elapsed_ms));

    return 0;
}
