#include "engine/timeman.hpp"
#include <algorithm>
#include <cstdlib>

namespace cordyceps {

GamePhase detect_phase(const Board& board) noexcept {
    int live = board.live_count;
    if (live > 32) return GamePhase::kOpening;
    if (live >= 20) return GamePhase::kMidgame;
    if (live > 12) return GamePhase::kLate;
    return GamePhase::kEndgame;
}

int estimate_moves_left(int live_count) noexcept {
    if (live_count > 60) return 22;
    if (live_count > 40) return 17;
    if (live_count > 25) return 12;
    if (live_count > 12) return 8;
    return 5;
}

float TimeManager::margin_factor(int margin) noexcept {
    if (margin > 40)  return 0.6f;
    if (margin > 20)  return 0.7f;
    if (margin > 5)   return 0.85f;
    if (margin > -5)  return 1.0f;
    if (margin > -20) return 1.2f;
    if (margin > -40) return 1.35f;
    return 1.5f;
}

static float phase_pct(int live_count, const SideConfig& config) noexcept {
    // Phase-based % of remaining per move (from log analysis of winning engines)
    // FIRST: 6% opening, 10% midgame, 12% late, 18% endgame
    // SECOND: adjusts via time_multiplier
    GamePhase phase;
    if (live_count > 32)      phase = GamePhase::kOpening;
    else if (live_count >= 20) phase = GamePhase::kMidgame;
    else if (live_count > 12)  phase = GamePhase::kLate;
    else                       phase = GamePhase::kEndgame;

    switch (phase) {
        case GamePhase::kOpening: return 6.0f;
        case GamePhase::kMidgame: return 10.0f;
        case GamePhase::kLate:    return 12.0f;
        case GamePhase::kEndgame: return 18.0f;
    }
    return 6.0f;
}

int TimeManager::get_budget(int live_count, const SideConfig& config,
                              int remaining_ms, int margin) const noexcept {
    // Emergency: <500ms remaining → fixed tiny budget
    if (remaining_ms < 500) {
        return 15;
    }

    // Base: % of remaining per move (from log analysis)
    float pct = phase_pct(live_count, config);
    float budget_f = remaining_ms * (pct / 100.0f);

    // Side multiplier (FIRST=1.0, SECOND=1.5)
    budget_f *= config.time_multiplier;

    // Margin factor (winning=save, losing=invest)
    budget_f *= margin_factor(margin);

    // Clamp to reasonable range
    if (budget_f < 10.0f) budget_f = 10.0f;

    // Generous cap: winning bots in logs spend up to 33%+
    // At 10000ms: SECOND 15% = 1500ms, so cap must allow this
    float max_budget = (live_count <= 12) ? 2500.0f : 2000.0f;
    if (budget_f > max_budget) budget_f = max_budget;

    // Hard limit: never use > 90% of remaining
    float hard_limit = remaining_ms * 0.9f;
    if (budget_f > hard_limit) budget_f = hard_limit;

    return static_cast<int>(budget_f);
}

} // namespace cordyceps
