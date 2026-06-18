#ifndef CORDYCEPS_ENGINE_BOARD_HPP
#define CORDYCEPS_ENGINE_BOARD_HPP

#include <cstdint>
#include <array>
#include "common/types.hpp"
#include "common/bitboard.hpp"

namespace cordyceps {

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
    std::array<std::int8_t, 16> changed_indices;
    std::array<std::int8_t, 16> old_values;
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
    [[nodiscard]] std::int8_t& value_at(int r, int c) noexcept { return values[r * k_cols + c]; }
    [[nodiscard]] const std::int8_t& value_at(int r, int c) const noexcept { return values[r * k_cols + c]; }
    [[nodiscard]] std::int8_t& owner_at(int r, int c) noexcept { return owners[r * k_cols + c]; }
    [[nodiscard]] const std::int8_t& owner_at(int r, int c) const noexcept { return owners[r * k_cols + c]; }

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
}

#endif // CORDYCEPS_ENGINE_BOARD_HPP
