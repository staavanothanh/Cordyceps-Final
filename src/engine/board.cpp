#include "engine/board.hpp"

namespace cordyceps {

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
    
    const int r1 = mv.r1, c1 = mv.c1, r2 = mv.r2, c2 = mv.c2;
    int change_idx = 0;
    int sum_collected = 0;

    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c) {
            const int idx = r * k_cols + c;
            if (values[idx] > 0) {
                undo.changed_indices[change_idx] = static_cast<std::int8_t>(idx);
                undo.old_values[change_idx] = values[idx];
                sum_collected += values[idx];
                values[idx] = 0;
                live_mask.clear(idx);
                --live_count;
                ++change_idx;
            }
        }
    }
    undo.changed_count = change_idx;

    // Update ownership bitboards and owners array
    for (int i = 0; i < change_idx; ++i) {
        const int idx = undo.changed_indices[i];
        owners[idx] = static_cast<std::int8_t>(current_player);
        if (current_player == k_player_us) {
            my_mask.set(idx);
        } else {
            opp_mask.set(idx);
        }
    }

    if (current_player == k_player_us) {
        my_score += sum_collected;
    } else {
        opp_score += sum_collected;
    }

    current_player = -current_player;
    return undo;
}

void Board::unmake_move(const UndoMove& undo) noexcept {
    if (!undo.mv.is_pass()) {
        // Restore values, owners, and bitboards
        for (int i = 0; i < undo.changed_count; ++i) {
            const int idx = undo.changed_indices[i];
            values[idx] = undo.old_values[i];
            owners[idx] = k_no_owner;
        }
    }

    // Restore full state
    my_mask = undo.old_my_mask;
    opp_mask = undo.old_opp_mask;
    live_mask = undo.old_live_mask;
    live_count = undo.old_live_count;
    my_score = undo.old_my_score;
    opp_score = undo.old_opp_score;
    consecutive_passes = undo.old_consecutive_passes;
    current_player = undo.old_current_player;
}

bool Board::is_terminal() const noexcept {
    return consecutive_passes >= 2;
}

int Board::score_from_perspective(int player) const noexcept {
    if (player == k_player_us) return my_score - opp_score;
    return opp_score - my_score;
}

} // namespace cordyceps
