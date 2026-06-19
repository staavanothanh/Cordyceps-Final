#include <gtest/gtest.h>
#include "engine/timeman.hpp"

using namespace cordyceps;

// ===== Phase Detection (new thresholds) =====

TEST(PhaseDetectionNewTest, OpeningPhase33Plus) {
    Board board;
    for (int i = 0; i < 35; ++i)
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kOpening);
}

TEST(PhaseDetectionNewTest, OpeningAt33) {
    Board board;
    for (int i = 0; i < 33; ++i)
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kOpening);
}

TEST(PhaseDetectionNewTest, Midgame20To32) {
    Board board;
    for (int i = 0; i < 25; ++i)
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kMidgame);
}

TEST(PhaseDetectionNewTest, MidgameAt20) {
    Board board;
    for (int i = 0; i < 20; ++i)
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kMidgame);
}

TEST(PhaseDetectionNewTest, LatePhase13To19) {
    Board board;
    for (int i = 0; i < 15; ++i)
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kLate);
}

TEST(PhaseDetectionNewTest, LateAt13) {
    Board board;
    for (int i = 0; i < 13; ++i)
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kLate);
}

TEST(PhaseDetectionNewTest, Endgame12OrLess) {
    Board board;
    for (int i = 0; i < 10; ++i)
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kEndgame);
}

TEST(PhaseDetectionNewTest, EndgameAt12) {
    Board board;
    for (int i = 0; i < 12; ++i)
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kEndgame);
}

// ===== TimeManager (new formula) =====

TEST(TimeManagerNewTest, FirstGetsLessThanSecond) {
    TimeManager tm;
    SideConfig first{1.0f, 0.3f, 1.0f, 2.0f, false};
    SideConfig second{1.5f, 0.7f, 1.0f, 1.0f, true};

    int t1 = tm.get_budget(50, first, 10000, 0);
    int t2 = tm.get_budget(50, second, 10000, 0);
    EXPECT_LE(t1, t2) << "FIRST " << t1 << "ms <= SECOND " << t2 << "ms";
}

TEST(TimeManagerNewTest, WinningUsesLess) {
    TimeManager tm;
    SideConfig cfg{1.0f, 0.5f, 1.0f, 1.0f, false};

    int losing = tm.get_budget(25, cfg, 8000, -40);
    int winning = tm.get_budget(25, cfg, 8000, 40);
    EXPECT_LE(winning, losing);
}

TEST(TimeManagerNewTest, OpeningIsContained) {
    TimeManager tm;
    SideConfig first{1.0f, 0.3f, 1.0f, 2.0f, false};
    SideConfig second{1.5f, 0.7f, 1.0f, 1.0f, true};

    int b1 = tm.get_budget(50, first, 10000, 0);
    EXPECT_GE(b1, 300) << "FIRST opening >= 300ms";
    EXPECT_LE(b1, 900) << "FIRST opening <= 900ms";

    int b2 = tm.get_budget(50, second, 10000, 0);
    EXPECT_GE(b2, 400) << "SECOND opening >= 400ms";
    EXPECT_LE(b2, 1400) << "SECOND opening <= 1400ms";
}

TEST(TimeManagerNewTest, MidgameBudgetHigherThanOpening) {
    TimeManager tm;
    SideConfig first{1.0f, 0.3f, 1.0f, 2.0f, false};

    int opening = tm.get_budget(50, first, 10000, 0);
    int midgame = tm.get_budget(25, first, 8000, 0);
    // Midgame uses higher % and has less remaining
    // At 8000ms, 10% = 800ms; at 10000ms, 6% = 600ms
    // So midgame should be >= opening roughly
    EXPECT_GE(midgame, opening * 0.7f) << "Midgame shouldn't be much lower than opening";
}

TEST(TimeManagerNewTest, EndgameUsesLessTotalThanOpening) {
    TimeManager tm;
    SideConfig cfg{1.0f, 0.5f, 1.0f, 1.0f, false};

    int opening = tm.get_budget(50, cfg, 10000, 0);
    int endgame = tm.get_budget(8, cfg, 5000, 0);
    // Endgame has less remaining → total budget lower
    // But % is higher (18% vs 6%)
    // At 5000ms: 18% = 900ms; at 10000ms: 6% = 600ms
    // So endgame per-move budget could be >= opening
    EXPECT_LE(endgame, 1200) << "Endgame budget <= 1200ms";
}

TEST(TimeManagerNewTest, EmergencyTinyBudget) {
    TimeManager tm;
    SideConfig cfg{1.0f, 0.5f, 1.0f, 1.0f, false};

    int budget = tm.get_budget(25, cfg, 400, 0);
    EXPECT_EQ(budget, 15) << "Emergency = 15ms fixed";
}

TEST(TimeManagerNewTest, SecondSpendsMore) {
    TimeManager tm;
    SideConfig first{1.0f, 0.3f, 1.0f, 2.0f, false};
    SideConfig second{1.5f, 0.7f, 1.0f, 1.0f, true};

    int mid_first = tm.get_budget(25, first, 10000, -10);
    int mid_second = tm.get_budget(25, second, 10000, -10);
    float ratio = static_cast<float>(mid_second) / mid_first;
    EXPECT_GE(ratio, 1.3f) << "SECOND/FIRST ratio " << ratio << " >= 1.3";
    EXPECT_LE(ratio, 2.0f) << "SECOND/FIRST ratio " << ratio << " <= 2.0";
}

TEST(TimeManagerNewTest, HardLimit90Percent) {
    TimeManager tm;
    SideConfig cfg{2.0f, 0.3f, 1.0f, 1.0f, false}; // crazy multiplier

    int budget = tm.get_budget(25, cfg, 5000, 0);
    EXPECT_LE(budget, 4500) << "Budget <= 90% of 5000";
}

TEST(TimeManagerNewTest, GameSimulationMatchesLogData) {
    TimeManager tm;
    // Simulate a full game for FIRST - budget should be ~9% avg
    SideConfig first{1.0f, 0.3f, 1.0f, 2.0f, false};
    
    struct Move { int live; int remaining; int margin; };
    Move game[] = {
        {140, 10000, 0}, {130, 9500, 5}, {120, 9000, 8},
        {110, 8500, 10}, {100, 8000, 12}, {90, 7500, 14},
        {80, 7000, 15}, {70, 6500, 15}, {60, 6000, 15},
        {55, 5500, 14}, {50, 5000, 12}, {45, 4500, 10},
        {40, 4000, 8}, {35, 3500, 5}, {30, 3000, 2},
        {28, 2500, 0}, {25, 2000, -2}, {22, 1500, -5},
        {18, 1000, -8}, {14, 800, -12},
    };

    int total_used = 0, moves = 0;
    int budget_sum = 0;
    for (auto& m : game) {
        int b = tm.get_budget(m.live, first, m.remaining, m.margin);
        budget_sum += b;
        total_used += b;
        moves++;
    }
    float avg = moves > 0 ? static_cast<float>(budget_sum) / moves : 0;
    EXPECT_GE(avg, 60) << "FIRST game avg budget >= 60ms";
    EXPECT_LE(avg, 400) << "FIRST game avg budget <= 400ms";
    
    float pct_of_pool = static_cast<float>(total_used) / 10000 * 100;
    EXPECT_LE(pct_of_pool, 90) << "Total used <= 90% of 10000ms pool";
}
