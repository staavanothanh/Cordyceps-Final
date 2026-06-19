#include "engine/timeman.hpp"

namespace cordyceps {

GamePhase detect_phase(const Board& board) noexcept {
    int live = board.live_count;
    if (live > 100) return GamePhase::kOpening;
    if (live > 50)  return GamePhase::kMidgame;
    if (live > 12)  return GamePhase::kLate;
    return GamePhase::kEndgame;
}

float TimeManager::phase_weight(GamePhase phase) noexcept {
    switch (phase) {
        case GamePhase::kOpening: return 0.3f;   // early: save time
        case GamePhase::kMidgame: return 0.8f;   // mid: normal
        case GamePhase::kLate:    return 1.2f;   // late: invest
        case GamePhase::kEndgame: return 1.5f;   // endgame: max
    }
    return 1.0f;
}

float TimeManager::margin_weight(int margin) noexcept {
    if (margin > 30)  return 0.5f;   // crushing: less time
    if (margin > 10)  return 0.7f;   // clearly winning
    if (margin > 0)   return 0.9f;   // slightly winning
    if (margin > -30) return 1.3f;   // losing: invest more
    return 1.5f;                       // badly losing: all in
}

int TimeManager::get_budget(GamePhase phase, const SideConfig& config,
                              int remaining_ms, int margin) const noexcept {
    // Cap savings
    float time_mult = config.time_multiplier;
    float ph = phase_weight(phase);
    float mg = margin_weight(margin);

    // Expected remaining moves: estimate based on live count
    float budget = remaining_ms * 0.08f;  // ~8% per move at start
    budget *= time_mult;
    budget *= ph;
    budget *= mg;

    // Clamp
    if (budget < 30)   budget = 30;
    if (budget > 9500) budget = 9500;

    // Hard stop: never use > 90% of remaining
    int hard_limit = remaining_ms * 9 / 10;
    if (budget > hard_limit) budget = static_cast<float>(hard_limit);

    return static_cast<int>(budget);
}

} // namespace cordyceps
