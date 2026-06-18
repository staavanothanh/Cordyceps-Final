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
            if (values[idx] > 0) {
                undo.changed_indices[change_idx] = static_cast<std::int8_t>(idx);
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

int evaluate(const Board& board, int player) noexcept {
    const auto& ec = board.eval_cache;

    int score = board.score_from_perspective(player);

    int territory_diff = ec.my_territory - ec.opp_territory;
    int corner_diff = ec.my_corners - ec.opp_corners;
    int edge_diff = ec.my_edges - ec.opp_edges;
    int adj_diff = ec.live_adj_my - ec.live_adj_opp;

    if (player == k_player_opp) {
        territory_diff = -territory_diff;
        corner_diff = -corner_diff;
        edge_diff = -edge_diff;
        adj_diff = -adj_diff;
    }

    return score * 10
         + territory_diff * 2
         + corner_diff * 5
         + edge_diff * 1
         + adj_diff * 1;
}

} // namespace cordyceps
