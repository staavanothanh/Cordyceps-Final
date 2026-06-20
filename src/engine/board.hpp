#ifndef CORDYCEPS_ENGINE_BOARD_HPP
#define CORDYCEPS_ENGINE_BOARD_HPP

#include <cstdint>
#include <array>
#include "common/types.hpp"
#include "common/bitboard.hpp"

namespace cordyceps {

struct EvalWeights {
    int score{3};
    int territory{3};
    int corners{5};
    int edges{1};
    int live_adj{3};
    int recapture{0};
    int vulnerability{0};
    int connectivity{1};  // NOTE: at END for backward-compat with 7-value config files
    int safe{2};

    [[nodiscard]] static constexpr EvalWeights baseline() noexcept {
        return EvalWeights{3, 3, 5, 1, 3, 0, 0, 1, 2};
    }
};

struct EvalCache {
    int my_territory{0};
    int opp_territory{0};
    int my_corners{0};
    int opp_corners{0};
    int my_edges{0};
    int opp_edges{0};
    int live_adj_my{0};
    int live_adj_opp{0};
    int connectivity_my{0};
    int connectivity_opp{0};
};

static_assert(sizeof(EvalCache) == 40, "EvalCache must be 40 bytes");

struct UndoMove {
    Move mv;
    int  changed_count{0};
    std::array<std::uint8_t, k_cells> changed_indices;
    std::array<std::int8_t, k_cells> old_values;
    Bitboard old_my_mask;
    Bitboard old_opp_mask;
    Bitboard old_live_mask;
    int old_live_count{0};
    int old_my_score{0};
    int old_opp_score{0};
    int old_consecutive_passes{0};
    int old_current_player{0};
    EvalCache old_eval_cache;
};

struct Board {
    // Grid data
    std::array<std::int8_t, k_cells> values{};   // 0=empty, 1-9=mushroom
    std::array<std::int8_t, k_cells> owners{};   // 0=none, 1=us, -1=opp

    // Bitboards
    Bitboard my_mask;
    Bitboard opp_mask;
    Bitboard live_mask;

    // Game state
    int my_score{0};
    int opp_score{0};
    int live_count{0};
    int current_player{0};          // 1=Cordyceps, -1=opponent
    int consecutive_passes{0};

    // Eval cache
    EvalCache eval_cache;

    // Cell access helpers
    [[nodiscard]] std::int8_t& value_at(int r, int c) noexcept { 
        if (r < 0 || r >= k_rows || c < 0 || c >= k_cols) return values[0];
        return values[r * k_cols + c]; 
    }
    [[nodiscard]] const std::int8_t& value_at(int r, int c) const noexcept { 
        if (r < 0 || r >= k_rows || c < 0 || c >= k_cols) return values[0];
        return values[r * k_cols + c]; 
    }
    [[nodiscard]] std::int8_t& owner_at(int r, int c) noexcept { 
        if (r < 0 || r >= k_rows || c < 0 || c >= k_cols) return owners[0];
        return owners[r * k_cols + c]; 
    }
    [[nodiscard]] const std::int8_t& owner_at(int r, int c) const noexcept { 
        if (r < 0 || r >= k_rows || c < 0 || c >= k_cols) return owners[0];
        return owners[r * k_cols + c]; 
    }

    // Operations
    void recalc_live_mask() noexcept;
    [[nodiscard]] UndoMove apply_move(const Move& mv) noexcept;
    void unmake_move(const UndoMove& undo) noexcept;
    [[nodiscard]] bool is_terminal() const noexcept;
    [[nodiscard]] int score_from_perspective(int player) const noexcept;

private:
    void save_old_state(UndoMove& undo) noexcept;
    void apply_move_on_board(const Move& mv, int player) noexcept;
};

} // namespace cordyceps

// Free function evaluate (side-agnostic)
namespace cordyceps {
[[nodiscard]] int evaluate(const Board& board, int player) noexcept;
[[nodiscard]] int evaluate(const Board& board, int player, const EvalWeights* weights) noexcept;

// Count safe territory cells: owned cells with NO adjacent opponent-owned cells
[[nodiscard]] int count_safe(const Board& board, int player) noexcept;

// Runtime weight loading for tuning (thread-local, zero-overhead when not set)
void set_tune_weights(int score_w, int territory_w, int corner_w, int edge_w,
                      int adj_w, int recapture_w, int vulnerability_w,
                      int connectivity_w = 1, int safe_w = 2) noexcept;
void clear_tune_weights() noexcept;

// Load eval weights from config file (format: "FIRST=w0..w6", "SECOND=w0..w6")
// Returns true on success, false if file not found/parse error.
// Output params unchanged on failure.
[[nodiscard]] bool load_weights_from_file(const char* path,
                                           EvalWeights& first_out,
                                           EvalWeights& second_out) noexcept;

// Deploy side-specific weights via set_tune_weights().
// FIRST uses first_weights, SECOND uses second_weights.
void deploy_side_weights(bool is_first,
                         const EvalWeights& first_weights,
                         const EvalWeights& second_weights) noexcept;
}

#endif // CORDYCEPS_ENGINE_BOARD_HPP
