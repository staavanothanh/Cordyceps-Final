#include <cstdio>
#include "engine/board.hpp"

namespace cordyceps {

static bool is_corner_cell(int r, int c) noexcept {
    return (r == 0 || r == k_rows - 1) && (c == 0 || c == k_cols - 1);
}

static bool is_edge_cell(int r, int c) noexcept {
    return r == 0 || r == k_rows - 1 || c == 0 || c == k_cols - 1;
}

void Board::recalc_live_mask() noexcept {
    live_mask = Bitboard::empty();
    live_count = 0;
    for (int i = 0; i < k_cells; ++i) {
        if (values[i] > 0) {
            live_mask.set(i);
            ++live_count;
        }
    }
}

void Board::save_old_state(UndoMove& undo) noexcept {
    undo.old_my_mask = my_mask;
    undo.old_opp_mask = opp_mask;
    undo.old_live_mask = live_mask;
    undo.old_live_count = live_count;
    undo.old_my_score = my_score;
    undo.old_opp_score = opp_score;
    undo.old_consecutive_passes = consecutive_passes;
    undo.old_current_player = current_player;
    undo.old_eval_cache = eval_cache;
}

UndoMove Board::apply_move(const Move& mv) noexcept {
    UndoMove undo;
    undo.mv = mv;
    save_old_state(undo);

    if (mv.is_pass()) {
        ++consecutive_passes;
        current_player = -current_player;
        return undo;
    }

    consecutive_passes = 0;

    EvalCache* ec = &eval_cache;
    int player = current_player;

    const int r1 = mv.r1, c1 = mv.c1, r2 = mv.r2, c2 = mv.c2;
    int change_idx = 0;
    int sum_collected = 0;

    // Live adj: track stale neighbors before clearing cells
    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c) {
            const int idx = r * k_cols + c;
            if (idx < 0 || idx >= k_cells) continue; // safety
            if (values[idx] > 0) {
                undo.changed_indices[change_idx] = static_cast<std::uint8_t>(idx);
                undo.old_values[change_idx] = values[idx];
                sum_collected += values[idx];

                // Decrement adjacent live counts for both players
                int dr[] = {-1, 1, 0, 0};
                int dc[] = {0, 0, -1, 1};
                for (int d = 0; d < 4; ++d) {
                    int nr = r + dr[d], nc = c + dc[d];
                    if (nr >= 0 && nr < k_rows && nc >= 0 && nc < k_cols) {
                        int nidx = nr * k_cols + nc;
                        if (owners[nidx] == k_player_us) {
                            if (ec->live_adj_my > 0) --ec->live_adj_my;
                        } else if (owners[nidx] == k_player_opp) {
                            if (ec->live_adj_opp > 0) --ec->live_adj_opp;
                        }
                    }
                }

                values[idx] = 0;
                live_mask.clear(idx);
                --live_count;
                ++change_idx;
            }
        }
    }
    undo.changed_count = change_idx;

    // Update ownership and eval
    for (int i = 0; i < change_idx; ++i) {
        const int idx = undo.changed_indices[i];
        int r = idx / k_cols, c = idx % k_cols;

        owners[idx] = static_cast<std::int8_t>(player);

        // Update connectivity: count edges to adjacent same-owner cells
        {
            int adj_dr[] = {-1, 1, 0, 0};
            int adj_dc[] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) {
                int nr2 = r + adj_dr[d], nc2 = c + adj_dc[d];
                if (nr2 >= 0 && nr2 < k_rows && nc2 >= 0 && nc2 < k_cols) {
                    int nidx2 = nr2 * k_cols + nc2;
                    if (owners[nidx2] == player) {
                        if (player == k_player_us) ++ec->connectivity_my;
                        else ++ec->connectivity_opp;
                    }
                }
            }
        }

        if (player == k_player_us) {
            my_mask.set(idx);
            ++ec->my_territory;
            if (is_corner_cell(r, c)) ++ec->my_corners;
            if (is_edge_cell(r, c)) ++ec->my_edges;
            my_score += undo.old_values[i];

            // Add adjacent live cells for my side
            int dr[] = {-1, 1, 0, 0};
            int dc[] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) {
                int nr = r + dr[d], nc = c + dc[d];
                if (nr >= 0 && nr < k_rows && nc >= 0 && nc < k_cols) {
                    int nidx = nr * k_cols + nc;
                    if (values[nidx] > 0 && owners[nidx] == k_no_owner)
                        ++ec->live_adj_my;
                }
            }
        } else {
            opp_mask.set(idx);
            ++ec->opp_territory;
            if (is_corner_cell(r, c)) ++ec->opp_corners;
            if (is_edge_cell(r, c)) ++ec->opp_edges;
            opp_score += undo.old_values[i];

            int dr[] = {-1, 1, 0, 0};
            int dc[] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) {
                int nr = r + dr[d], nc = c + dc[d];
                if (nr >= 0 && nr < k_rows && nc >= 0 && nc < k_cols) {
                    int nidx = nr * k_cols + nc;
                    if (values[nidx] > 0 && owners[nidx] == k_no_owner)
                        ++ec->live_adj_opp;
                }
            }
        }
    }

    current_player = -current_player;
    return undo;
}

void Board::unmake_move(const UndoMove& undo) noexcept {
    if (!undo.mv.is_pass()) {
        for (int i = 0; i < undo.changed_count; ++i) {
            const int idx = undo.changed_indices[i];
            values[idx] = undo.old_values[i];
            owners[idx] = k_no_owner;
        }
    }

    my_mask = undo.old_my_mask;
    opp_mask = undo.old_opp_mask;
    live_mask = undo.old_live_mask;
    live_count = undo.old_live_count;
    my_score = undo.old_my_score;
    opp_score = undo.old_opp_score;
    consecutive_passes = undo.old_consecutive_passes;
    current_player = undo.old_current_player;
    eval_cache = undo.old_eval_cache;
}

bool Board::is_terminal() const noexcept {
    return consecutive_passes >= 2;
}

int Board::score_from_perspective(int player) const noexcept {
    if (player == k_player_us) return my_score - opp_score;
    return opp_score - my_score;
}

// ===== Runtime weight loading for tuning =====
// Thread-local: zero overhead when not in tune mode
// Order: score, territory, corners, edges, live_adj, recapture, vulnerability
static thread_local int g_tune_w0 = 0;
static thread_local int g_tune_w1 = 0;
static thread_local int g_tune_w2 = 0;
static thread_local int g_tune_w3 = 0;
static thread_local int g_tune_w4 = 0;
static thread_local int g_tune_w5 = 0;
static thread_local int g_tune_w6 = 0;
static thread_local bool g_tune_active = false;

void set_tune_weights(int score_w, int territory_w, int corner_w, int edge_w,
                      int adj_w, int recapture_w, int vulnerability_w) noexcept {
    g_tune_w0 = score_w;
    g_tune_w1 = territory_w;
    g_tune_w2 = corner_w;
    g_tune_w3 = edge_w;
    g_tune_w4 = adj_w;
    g_tune_w5 = recapture_w;
    g_tune_w6 = vulnerability_w;
    g_tune_active = true;
}

void clear_tune_weights() noexcept {
    g_tune_active = false;
}

bool load_weights_from_file(const char* path,
                             EvalWeights& first_out,
                             EvalWeights& second_out) noexcept {
    FILE* f = std::fopen(path, "r");
    if (!f) return false;

    char line[128];
    int parsed_ok = 0;
    while (std::fgets(line, sizeof(line), f)) {
        // Skip comments/empty
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        int w[9];
        // Try 9-value format first: score territory corners edges live_adj recapture vulnerability connectivity safe
        if (std::sscanf(line, "FIRST=%d %d %d %d %d %d %d %d %d",
                        &w[0], &w[1], &w[2], &w[3], &w[4], &w[5], &w[6], &w[7], &w[8]) == 9) {
            first_out = {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8]};
            parsed_ok++;
        } else if (std::sscanf(line, "FIRST=%d %d %d %d %d %d %d %d",
                               &w[0], &w[1], &w[2], &w[3], &w[4], &w[5], &w[6], &w[7]) == 8) {
            // 8-value: safe defaults to 2
            first_out = {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], 2};
            parsed_ok++;
        } else if (std::sscanf(line, "FIRST=%d %d %d %d %d %d %d",
                               &w[0], &w[1], &w[2], &w[3], &w[4], &w[5], &w[6]) == 7) {
            // Backward compat: 7-value, connectivity=1, safe=2
            first_out = {w[0], w[1], w[2], w[3], w[4], w[5], w[6], 1, 2};
            parsed_ok++;
        } else if (std::sscanf(line, "SECOND=%d %d %d %d %d %d %d %d %d",
                               &w[0], &w[1], &w[2], &w[3], &w[4], &w[5], &w[6], &w[7], &w[8]) == 9) {
            second_out = {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8]};
            parsed_ok++;
        } else if (std::sscanf(line, "SECOND=%d %d %d %d %d %d %d %d",
                               &w[0], &w[1], &w[2], &w[3], &w[4], &w[5], &w[6], &w[7]) == 8) {
            // 8-value: safe defaults to 2
            second_out = {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], 2};
            parsed_ok++;
        } else if (std::sscanf(line, "SECOND=%d %d %d %d %d %d %d",
                               &w[0], &w[1], &w[2], &w[3], &w[4], &w[5], &w[6]) == 7) {
            second_out = {w[0], w[1], w[2], w[3], w[4], w[5], w[6], 1, 2};
            second_out = {w[0], w[1], w[2], w[3], w[4], w[5], w[6]};
            parsed_ok++;
        }
    }
    std::fclose(f);
    return parsed_ok >= 2;
}

void deploy_side_weights(bool is_first,
                          const EvalWeights& first_weights,
                          const EvalWeights& second_weights) noexcept {
    const auto& w = is_first ? first_weights : second_weights;
    set_tune_weights(w.score, w.territory, w.corners, w.edges,
                     w.live_adj, w.recapture, w.vulnerability);
}

// ===== Safe territory: count owned cells with NO adjacent opponent cells =====
int count_safe(const Board& board, int player) noexcept {
    int safe = 0;
    for (int i = 0; i < k_cells; ++i) {
        if (board.owners[i] != player) continue;
        int r = i / k_cols, c = i % k_cols;
        bool has_opp = false;
        int dr[] = {-1, 1, 0, 0};
        int dc[] = {0, 0, -1, 1};
        for (int d = 0; d < 4 && !has_opp; ++d) {
            int nr = r + dr[d], nc = c + dc[d];
            if (nr < 0 || nr >= k_rows || nc < 0 || nc >= k_cols) continue;
            if (board.owners[nr * k_cols + nc] == -player) has_opp = true;
        }
        if (!has_opp) ++safe;
    }
    return safe;
}

int evaluate(const Board& board, int player, const EvalWeights* weights) noexcept {
    const auto& ec = board.eval_cache;

    int score = board.score_from_perspective(player);

    int territory_diff = ec.my_territory - ec.opp_territory;
    int corner_diff = ec.my_corners - ec.opp_corners;
    int edge_diff = ec.my_edges - ec.opp_edges;
    int adj_diff = ec.live_adj_my - ec.live_adj_opp;
    int conn_diff = ec.connectivity_my - ec.connectivity_opp;

    // Compute safe cells (O(k_cells), no EvalCache dependency)
    int safe_my = count_safe(board, k_player_us);
    int safe_opp = count_safe(board, k_player_opp);
    int safe_diff = safe_my - safe_opp;

    if (player == k_player_opp) {
        territory_diff = -territory_diff;
        corner_diff = -corner_diff;
        edge_diff = -edge_diff;
        adj_diff = -adj_diff;
        conn_diff = -conn_diff;
        safe_diff = -safe_diff;
    }

    if (weights) {
        return score * weights->score
             + territory_diff * weights->territory
             + corner_diff * weights->corners
             + edge_diff * weights->edges
             + adj_diff * weights->live_adj
             + conn_diff * weights->connectivity
             + safe_diff * weights->safe;
    }

    // Use runtime weights if active (tuner mode)
    if (g_tune_active) {
        return score * g_tune_w0
             + territory_diff * g_tune_w1
             + corner_diff * g_tune_w2
             + edge_diff * g_tune_w3
             + adj_diff * g_tune_w4
             + conn_diff * 1  // connectivity fixed at 1 during tuning
             + safe_diff * 2; // safe cells fixed at 2 during tuning
    }

    {
        constexpr auto b = EvalWeights::baseline();
        return score * b.score
             + territory_diff * b.territory
             + corner_diff * b.corners
             + edge_diff * b.edges
             + adj_diff * b.live_adj
             + conn_diff * b.connectivity
             + safe_diff * b.safe;
    }
}

int evaluate(const Board& board, int player) noexcept {
    // Delegate to the overload with nullptr (uses tune weights or baseline)
    return evaluate(board, player, static_cast<const EvalWeights*>(nullptr));
}

} // namespace cordyceps
