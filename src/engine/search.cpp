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

    // Killer moves get bonus
    if (mv == killer1_[depth]) return 200;
    if (mv == killer2_[depth]) return 100;

    // History heuristic: successful moves get priority
    int h = history_[mv.r1][mv.c1][mv.r2][mv.c2];
    if (h > 0) return 50 + h;

    // Side-aware: vertical vs horizontal preference
    if (prefer_vertical_) {
        int height = mv.r2 - mv.r1 + 1;
        int width = mv.c2 - mv.c1 + 1;
        if (height > width) return 25;  // vertical bonus
    } else if (aggression_ < 0.4f) {
        int height = mv.r2 - mv.r1 + 1;
        int width = mv.c2 - mv.c1 + 1;
        if (width >= height) return 25; // horizontal bonus (FIRST, defensive)
    }

    // Score: total mushroom value in the rect
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

// ===== Negamax α-β =====

static constexpr int MAX_DEPTH = 64; // increased for deeper searches
static constexpr int INF = 999999;

int Search::negamax(Board& board, int depth, int alpha, int beta, bool allow_pass) noexcept {
    if (timed_out_) return 0;
    if (!time_check()) return 0;

    bool terminal = board.is_terminal();

    // Quiescence-like: at depth 0, evaluate from current player's perspective
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

    // Null-move pruning (depth >= 3, also guards against 2 consecutive passes)
    if (allow_pass && depth >= 3 && !terminal && board.consecutive_passes < 1) {
        auto undo = board.apply_move(k_pass_move);
        int score = -negamax(board, depth - 1 - 2, -beta, -beta + 1, false);
        board.unmake_move(undo);
        if (score >= beta) return beta;
    }

    // Generate moves
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

        // LMR: reduce late moves at sufficient depth
        if (searched >= 4 && depth >= 3 && !mv.is_pass() && mv != tt_move) {
            // R = 1 + (searched / 4), cap at depth / 2
            int R = 1 + (searched / 4);
            if (R > depth / 2) R = depth / 2;
            if (R < 1) R = 1;

            // Null-window search at reduced depth
            score = -negamax(board, depth - 1 - R, -alpha - 1, -alpha, true);
            if (score > alpha && score < beta) {
                // Re-search full depth & window
                score = -negamax(board, depth - 1, -beta, -alpha, true);
                is_full_search = true;
            }
        } else {
            // Regular full search
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
            // Store killer (only for quiet/capture moves, not PASS)
            if (!mv.is_pass()) {
                if (mv != killer1_[depth]) {
                    killer2_[depth] = killer1_[depth];
                    killer1_[depth] = mv;
                }
                // Update history: successful moves get depth^2 bonus
                if (is_full_search) {
                    history_[mv.r1][mv.c1][mv.r2][mv.c2] += depth * depth;
                }
            }
            break;
        }
    }

    // Update history: negative bonus for failed moves (penalty)
    if (best_move != moves[0] && !best_move.is_pass() && !moves[0].is_pass()) {
        // The first move failed → demote it in history
        history_[moves[0].r1][moves[0].c1][moves[0].r2][moves[0].c2] -= depth;
    }

    // TT store
    TTEntry::Flag flag;
    if (best_score <= alpha_orig) flag = TTEntry::ALPHA;
    else if (best_score >= beta) flag = TTEntry::BETA;
    else flag = TTEntry::EXACT;
    tt_.store(hash, depth, flag, best_score, best_move);

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

    // Store side config
    prefer_vertical_ = config.prefer_vertical;
    aggression_ = config.aggression;
    steal_bonus_ = config.steal_bonus;
    defense_bonus_ = config.defense_bonus;

    // Clear history and killers
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

    if (board.consecutive_passes >= 1) {
        int margin = board.score_from_perspective(board.current_player);
        if (margin > 0) {
            return {k_pass_move, best_eval};
        }
    }

    best_move = moves[0];

    GamePhase phase = detect_phase(board);
    bool endgame = (phase == GamePhase::kEndgame);
    int max_d = endgame ? 64 : MAX_DEPTH; // endgame: enable extremely deep search

    int last_eval = 0;
    for (int d = 1; d <= max_d && !timed_out_; ++d) {
        max_depth_reached_ = d;
        int alpha = last_eval - 100;
        int beta = last_eval + 100;

        int score = negamax(board, d, alpha, beta, true);
        if (timed_out_) break;

        // Re-search if outside window (wider window for speed)
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
            best_move = tt_move;
        }
    }

    return {best_move, best_eval, max_depth_reached_, tt_probes_, tt_hits_, node_count_};
}

// ===== Simple search =====

SearchResult Search::simple_search(const Board& board, [[maybe_unused]] const SideConfig& config) noexcept {
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
