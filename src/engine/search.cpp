#include "engine/search.hpp"
#include "engine/timeman.hpp"
#include <algorithm>
#include <random>
#include <cstring>

namespace cordyceps {

// ===== Time management =====

bool Search::time_check() noexcept {
    if (++node_count_ % 1024 == 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time_).count();
        if (elapsed >= time_limit_ms_) {
            timed_out_ = true;
            return false;
        }
    }
    return true;
}

// ===== Move ordering =====

int Search::order_score(const Board& board, const Move& mv, int depth) noexcept {
    if (mv.is_pass()) return -1;

    if (mv == killer1_[depth]) return 200;
    if (mv == killer2_[depth]) return 100;

    int h = history_[mv.r1][mv.c1][mv.r2][mv.c2];
    if (h > 0) return 50 + h;

    // Steal detection via precomputed border masks (O(1), no cell scan)
    if (steal_bonus_ > 0.1f) {
        int id = table_.rect_id(mv.r1, mv.c1, mv.r2, mv.c2);
        const auto& rect = table_.get_rect(id);
        Bitboard border = rect.top_mask;
        border |= rect.bottom_mask;
        border |= rect.left_mask;
        border |= rect.right_mask;
        border &= board.opp_mask;
        int steal_count = border.popcount();
        if (steal_count > 0) {
            int bonus = static_cast<int>(steal_count * 50 * steal_bonus_);
            return 1000 + bonus;
        }
    }

    if (prefer_vertical_) {
        int height = mv.r2 - mv.r1 + 1;
        int width = mv.c2 - mv.c1 + 1;
        if (height > width) return 25;
    } else if (aggression_ < 0.4f) {
        int height = mv.r2 - mv.r1 + 1;
        int width = mv.c2 - mv.c1 + 1;
        if (width >= height) return 25;
    }

    int sum = 0;
    for (int r = mv.r1; r <= mv.r2; ++r)
        for (int c = mv.c1; c <= mv.c2; ++c)
            sum += board.value_at(r, c);
    return sum;
}

void Search::sort_moves(Board& board, std::vector<Move>& moves, int depth, const Move& tt_move) noexcept {
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        int sa = order_score(board, a, depth);
        int sb = order_score(board, b, depth);
        if (a == tt_move) sa = 10000;
        if (b == tt_move) sb = 10000;
        return sa > sb;
    });
}

// ===== Root-level geometry enhancement =====

void Search::enhance_root_moves(Board& board, std::vector<Move>& moves, int player) noexcept {
    // Score each root move with static eval + geometry features
    // This improves initial best_move selection without search-speed penalty
    for (auto& mv : moves) {
        if (mv.is_pass()) continue;

        auto undo = board.apply_move(mv);
        int eval_score = evaluate(board, player);
        board.unmake_move(undo);

        // Mobility bonus: how many legal moves does opponent have after our move?
        auto opp_moves = generate_legal_moves_optimized(board, table_);
        int mobility_bonus = -static_cast<int>(opp_moves.size()) / 3;
        eval_score += mobility_bonus;

        // Steal bonus: does this rect contain any opponent cells?
        int rect_id_val = table_.rect_id(mv.r1, mv.c1, mv.r2, mv.c2);
        auto cells = table_.get_cells(rect_id_val);
        int opp_cells = 0;
        for (auto cidx : cells) {
            if (board.owners[cidx] == -player) ++opp_cells;
        }
        eval_score += opp_cells * 2;  // steal bonus

        mv.score_hint = eval_score;
    }
}

// ===== Futility pruning check =====

static bool is_futile(const Board&, int, int) noexcept {
    return false;
}

// ===== Negamax (original, no geometry — for backward compat) =====

static constexpr int MAX_DEPTH = 64;
static constexpr int INF = 999999;

int Search::negamax(Board& board, int depth, int alpha, int beta, bool allow_pass) noexcept {
    if (timed_out_) return 0;
    if (!time_check()) return 0;

    bool terminal = board.is_terminal();
    if (depth <= 0 || terminal) {
        return evaluate(board, board.current_player);
    }

    // TT probe
    ++tt_probes_;
    std::uint64_t hash = zobrist_.compute(board);
    int tt_score;
    Move tt_move;
    auto tt_flag = tt_.probe(hash, depth, tt_score, tt_move);
    if (tt_flag != TTEntry::EMPTY) ++tt_hits_;
    if (tt_flag == TTEntry::EXACT) return tt_score;
    if (tt_flag == TTEntry::ALPHA && tt_score <= alpha) return alpha;
    if (tt_flag == TTEntry::BETA && tt_score >= beta) return beta;

    // Futility pruning: skip unpromising branches at shallow depth
    if (is_futile(board, depth, alpha)) {
        return evaluate(board, board.current_player);
    }

    // Null-move pruning
    if (allow_pass && depth >= 3 && !terminal && board.consecutive_passes < 1) {
        auto undo = board.apply_move(k_pass_move);
        int score = -negamax(board, depth - 1 - 2, -beta, -beta + 1, false);
        board.unmake_move(undo);
        if (score >= beta) return beta;
    }

    auto moves = generate_legal_moves_optimized(board, table_);
    moves.push_back(k_pass_move);
    sort_moves(board, moves, depth, tt_move);

    int best_score = -INF;
    Move best_move = k_pass_move;
    int alpha_orig = alpha;
    int searched = 0;

    for (int i = 0; i < static_cast<int>(moves.size()); ++i) {
        const auto& mv = moves[i];
        auto undo = board.apply_move(mv);

        int score;
        bool is_full_search = false;

        if (searched >= 4 && depth >= 3 && !mv.is_pass() && mv != tt_move) {
            int R = 1 + (searched / 4);
            if (R > depth / 2) R = depth / 2;
            if (R < 1) R = 1;

            score = -negamax(board, depth - 1 - R, -alpha - 1, -alpha, true);
            if (score > alpha && score < beta) {
                score = -negamax(board, depth - 1, -beta, -alpha, true);
                is_full_search = true;
            }
        } else {
            score = -negamax(board, depth - 1, -beta, -alpha, true);
            is_full_search = true;
        }

        board.unmake_move(undo);

        if (score > best_score) {
            best_score = score;
            best_move = mv;
        }
        ++searched;

        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            if (!mv.is_pass()) {
                if (mv != killer1_[depth]) {
                    killer2_[depth] = killer1_[depth];
                    killer1_[depth] = mv;
                }
                if (is_full_search) {
                    history_[mv.r1][mv.c1][mv.r2][mv.c2] += depth * depth;
                }
            }
            break;
        }
    }

    if (best_move != moves[0] && !best_move.is_pass() && !moves[0].is_pass()) {
        history_[moves[0].r1][moves[0].c1][moves[0].r2][moves[0].c2] -= depth;
    }

    TTEntry::Flag flag;
    if (best_score <= alpha_orig) flag = TTEntry::ALPHA;
    else if (best_score >= beta) flag = TTEntry::BETA;
    else flag = TTEntry::EXACT;
    tt_.store(hash, depth, flag, best_score, best_move);

    return best_score;
}

// ===== Endgame exact solver =====

int Search::negamax_endgame(Board& board, int alpha, int beta, bool allow_pass) noexcept {
    if (timed_out_) return 0;
    if (!time_check()) return 0;

    bool terminal = board.is_terminal();

    // Generate moves
    auto moves = generate_legal_moves_optimized(board, table_);

    if (terminal) {
        return evaluate(board, board.current_player);
    }

    // If no legal moves, must pass
    if (moves.empty()) {
        if (!allow_pass) {
            // Can't pass (previous move was already a pass)
            return evaluate(board, board.current_player);
        }
        auto undo = board.apply_move(k_pass_move);
        int score = -negamax_endgame(board, -beta, -alpha, true);
        board.unmake_move(undo);
        return score;
    }

    // TT probe with max depth
    ++tt_probes_;
    std::uint64_t hash = zobrist_.compute(board);
    int tt_score;
    Move tt_move;
    auto tt_flag = tt_.probe(hash, 64, tt_score, tt_move);
    if (tt_flag != TTEntry::EMPTY) ++tt_hits_;
    if (tt_flag == TTEntry::EXACT) return tt_score;
    if (tt_flag == TTEntry::ALPHA && tt_score <= alpha) return alpha;
    if (tt_flag == TTEntry::BETA && tt_score >= beta) return beta;

    // Null-move pruning with reduced R in endgame
    if (allow_pass && board.consecutive_passes < 1 && moves.size() > 1) {
        auto undo = board.apply_move(k_pass_move);
        int score = -negamax_endgame(board, -beta, -beta + 1, false);
        board.unmake_move(undo);
        if (score >= beta) return beta;
    }

    moves.push_back(k_pass_move);
    sort_moves(board, moves, 64, tt_move);

    int best_score = -INF;
    Move best_move = k_pass_move;
    int alpha_orig = alpha;

    for (int i = 0; i < static_cast<int>(moves.size()); ++i) {
        const auto& mv = moves[i];
        auto undo = board.apply_move(mv);

        int score;
        if (i == 0) {
            // Full search for PV move
            score = -negamax_endgame(board, -beta, -alpha, mv.is_pass());
        } else {
            // Zero window search for remaining moves
            score = -negamax_endgame(board, -alpha - 1, -alpha, mv.is_pass());
            if (score > alpha && score < beta) {
                score = -negamax_endgame(board, -beta, -alpha, mv.is_pass());
            }
        }

        board.unmake_move(undo);

        if (score > best_score) {
            best_score = score;
            best_move = mv;
        }

        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            break;
        }
    }

    TTEntry::Flag flag;
    if (best_score <= alpha_orig) flag = TTEntry::ALPHA;
    else if (best_score >= beta) flag = TTEntry::BETA;
    else flag = TTEntry::EXACT;
    tt_.store(hash, 64, flag, best_score, best_move);

    return best_score;
}

// ===== Iterative Deepening =====

SearchResult Search::iterative_deepening(Board& board, int time_ms, const SideConfig& config) noexcept {
    start_time_ = std::chrono::steady_clock::now();
    time_limit_ms_ = time_ms;
    timed_out_ = false;
    node_count_ = 0;
    tt_probes_ = 0;
    tt_hits_ = 0;
    max_depth_reached_ = 0;
    tt_.clear();

    prefer_vertical_ = config.prefer_vertical;
    aggression_ = config.aggression;
    steal_bonus_ = config.steal_bonus;
    defense_bonus_ = config.defense_bonus;

    std::memset(history_, 0, sizeof(history_));
    for (int i = 0; i < 64; ++i) {
        killer1_[i] = {};
        killer2_[i] = {};
    }

    Move best_move = k_pass_move;
    int best_eval = evaluate(board, board.current_player);

    auto moves = generate_legal_moves_optimized(board, table_);
    if (moves.empty()) {
        return {k_pass_move, best_eval};
    }

    // Root-level geometry enhancement: score moves with eval + geometry
    enhance_root_moves(board, moves, board.current_player);

    // Sort by geometry-enhanced score
    std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) {
        return a.score_hint > b.score_hint;
    });

    if (board.consecutive_passes >= 1) {
        int margin = board.score_from_perspective(board.current_player);
        if (margin > 0) {
            return {k_pass_move, best_eval};
        }
    }

    best_move = moves[0];

    GamePhase phase = detect_phase(board);
    bool endgame = (phase == GamePhase::kEndgame);

    if (endgame && board.live_count <= 12) {
        // Deep endgame: exact solver
        negamax_endgame(board, -INF, INF, true);
        if (timed_out_ && best_move.is_pass()) {
            best_move = moves[0];
        }
        std::uint64_t hash = zobrist_.compute(board);
        int tt_score;
        Move tt_move;
        auto flag = tt_.probe(hash, 64, tt_score, tt_move);
        if (flag != TTEntry::EMPTY && !tt_move.is_pass()) {
            bool valid = false;
            for (const auto& mv : moves) {
                if (mv.r1 == tt_move.r1 && mv.c1 == tt_move.c1 &&
                    mv.r2 == tt_move.r2 && mv.c2 == tt_move.c2) {
                    valid = true;
                    break;
                }
            }
            if (valid) {
                best_move = tt_move;
                best_eval = tt_score;
            }
        }
        max_depth_reached_ = 64;
        return {best_move, best_eval, max_depth_reached_, tt_probes_, tt_hits_, node_count_};
    }

    // Iterative deepening with progressive widening
    int max_d = MAX_DEPTH;
    int last_eval = 0;

    for (int d = 1; d <= max_d && !timed_out_; ++d) {
        max_depth_reached_ = d;
        int alpha = last_eval - 100;
        int beta = last_eval + 100;

        // Using regular negamax — negamax_geo available for future optimization
        int score = negamax(board, d, alpha, beta, true);
        if (timed_out_) break;

        if (score <= alpha) {
            score = negamax(board, d, -INF, beta, true);
        } else if (score >= beta) {
            score = negamax(board, d, alpha, INF, true);
        }
        if (timed_out_) break;

        last_eval = score;
        best_eval = score;

        std::uint64_t hash = zobrist_.compute(board);
        int tt_score;
        Move tt_move;
        auto flag = tt_.probe(hash, d, tt_score, tt_move);
        if (tt_move != k_pass_move && !tt_move.is_pass() && flag != TTEntry::EMPTY) {
            bool valid = false;
            for (const auto& mv : moves) {
                if (mv.r1 == tt_move.r1 && mv.c1 == tt_move.c1 &&
                    mv.r2 == tt_move.r2 && mv.c2 == tt_move.c2) {
                    valid = true;
                    break;
                }
            }
            if (valid) best_move = tt_move;
        }
    }

    return {best_move, best_eval, max_depth_reached_, tt_probes_, tt_hits_, node_count_};
}

// ===== Simple search =====

SearchResult Search::simple_search(const Board& board, const SideConfig&) noexcept {
    auto moves = generate_legal_moves_optimized(board, table_);
    int player = board.current_player;

    SearchResult best{k_pass_move, -999999};

    for (const auto& mv : moves) {
        Board tmp = board;
        tmp.current_player = player;
        auto undo = tmp.apply_move(mv);
        int score = evaluate(tmp, player);
        tmp.unmake_move(undo);

        if (score > best.eval) {
            best.eval = score;
            best.move = mv;
        }
    }

    if (moves.empty()) {
        return {k_pass_move, evaluate(board, player)};
    }

    if (board.consecutive_passes >= 1) {
        int margin = board.score_from_perspective(player);
        if (margin > 0) {
            return {k_pass_move, evaluate(board, player)};
        }
    }

    return best;
}

// ===== Benchmark =====

SearchBenchmark Search::benchmark(const RectTable& table, const Zobrist& zobrist, int time_ms, int samples) noexcept {
    SearchBenchmark bm{};
    bm.samples = samples;

    for (int s = 0; s < samples; ++s) {
        Board board;
        int target_live = 60 + (s * 7) % 50;
        for (int i = 0; i < target_live; ++i) {
            int idx = (i * 37 + s * 13) % k_cells;
            int val = 1 + ((i + s) % 9);
            board.values[idx] = static_cast<std::int8_t>(val);
        }
        board.current_player = (s % 2 == 0) ? k_player_us : k_player_opp;
        board.recalc_live_mask();

        Search search(table, zobrist);
        auto start = std::chrono::steady_clock::now();
        auto result = search.iterative_deepening(board, time_ms, {});
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        bm.avg_depth += result.max_depth;
        bm.avg_nodes += result.nodes;
        bm.avg_ms += elapsed;
        if (result.tt_probes > 0)
            bm.avg_hit_rate += static_cast<double>(result.tt_hits) / result.tt_probes * 100.0;
    }

    bm.avg_depth /= samples;
    bm.avg_nodes /= samples;
    bm.avg_ms /= samples;
    bm.avg_hit_rate /= samples;
    return bm;
}

} // namespace cordyceps
