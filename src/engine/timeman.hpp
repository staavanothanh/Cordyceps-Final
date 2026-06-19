#ifndef CORDYCEPS_ENGINE_TIMEMAN_HPP
#define CORDYCEPS_ENGINE_TIMEMAN_HPP

#include "common/types.hpp"
#include "engine/board.hpp"

namespace cordyceps {

// Detect game phase based on live_count
[[nodiscard]] GamePhase detect_phase(const Board& board) noexcept;

class TimeManager {
public:
    TimeManager() noexcept = default;

    // Calculate search budget for this turn
    // remaining_ms: total remaining game time (from TIME message)
    // margin: current score from perspective of player (positive = winning)
    [[nodiscard]] int get_budget(GamePhase phase, const SideConfig& config,
                                  int remaining_ms, int margin) const noexcept;

private:
    [[nodiscard]] static float phase_weight(GamePhase phase) noexcept;
    [[nodiscard]] static float margin_weight(int margin) noexcept;
};

} // namespace cordyceps

#endif // CORDYCEPS_ENGINE_TIMEMAN_HPP
