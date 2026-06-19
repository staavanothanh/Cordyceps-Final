#ifndef CORDYCEPS_COMMON_TYPES_HPP
#define CORDYCEPS_COMMON_TYPES_HPP

#include <cstdint>
#include <array>

namespace cordyceps {

constexpr int k_rows = 10;
constexpr int k_cols = 17;
constexpr int k_cells = k_rows * k_cols; // 170
constexpr int k_target_sum = 10;
constexpr int k_num_rects = 8415; // all possible rects on 10x17 grid

constexpr int k_player_us = 1;
constexpr int k_player_opp = -1;
constexpr int k_no_owner = 0;

struct Move {
    std::int8_t r1;
    std::int8_t c1;
    std::int8_t r2;
    std::int8_t c2;

    [[nodiscard]] bool is_pass() const noexcept { return r1 == -1; }

    bool operator==(const Move& other) const noexcept {
        return r1 == other.r1 && c1 == other.c1 
            && r2 == other.r2 && c2 == other.c2;
    }

    bool operator!=(const Move& other) const noexcept {
        return !(*this == other);
    }
};

constexpr Move k_pass_move{-1, -1, -1, -1};

enum class GamePhase { kOpening, kMidgame, kLate, kEndgame };

struct SideConfig {
    float time_multiplier;
    float aggression;
    float steal_bonus;
    float defense_bonus;
    bool  prefer_vertical;
};

} // namespace cordyceps

#endif // CORDYCEPS_COMMON_TYPES_HPP
