#ifndef CORDYCEPS_ENGINE_TIMEMAN_HPP
#define CORDYCEPS_ENGINE_TIMEMAN_HPP

#include "common/types.hpp"
#include "engine/board.hpp"

namespace cordyceps {

// Detect game phase based on live_count (mushroom-bot thresholds)
// live > 32  -> Opening
// 20-32      -> Midgame
// 13-19      -> Late
// ≤ 12       -> Endgame
[[nodiscard]] GamePhase detect_phase(const Board& board) noexcept;
[[nodiscard]] int estimate_moves_left(int live_count) noexcept;

class TimeManager {
public:
    TimeManager() noexcept = default;

    // Per-move budget = remaining_ms * phase_pct / 100 * side_mult * margin_factor
    // phase_pct: 6% opening, 10% midgame, 12% late, 18% endgame
    [[nodiscard]] int get_budget(int live_count, const SideConfig& config,
                                  int remaining_ms, int margin) const noexcept;

private:
    [[nodiscard]] static float margin_factor(int margin) noexcept;
};

} // namespace cordyceps

#endif // CORDYCEPS_ENGINE_TIMEMAN_HPP
