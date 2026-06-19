#include <gtest/gtest.h>
#include "engine/timeman.hpp"

using namespace cordyceps;

TEST(PhaseDetectionTest, OpeningPhase) {
    Board board;
    for (int i = 0; i < 110; ++i) {
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    }
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kOpening);
}

TEST(PhaseDetectionTest, MidgamePhase) {
    Board board;
    for (int i = 0; i < 70; ++i) {
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    }
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kMidgame);
}

TEST(PhaseDetectionTest, LatePhase) {
    Board board;
    for (int i = 0; i < 30; ++i) {
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    }
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kLate);
}

TEST(PhaseDetectionTest, EndgamePhase) {
    Board board;
    for (int i = 0; i < 10; ++i) {
        board.values[i] = static_cast<std::int8_t>(1 + (i % 9));
    }
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kEndgame);
}

TEST(PhaseDetectionTest, EmptyBoardEndgame) {
    Board board;
    board.recalc_live_mask();
    EXPECT_EQ(detect_phase(board), GamePhase::kEndgame);
}

TEST(TimeManagerTest, FirstGetsLess) {
    TimeManager tm;
    SideConfig first{1.0f, 0.3f, 1.0f, 2.0f, false};
    SideConfig second{1.5f, 0.7f, 1.0f, 1.0f, true};

    int t1 = tm.get_budget(GamePhase::kMidgame, first, 5000, 0);
    int t2 = tm.get_budget(GamePhase::kMidgame, second, 5000, 0);
    EXPECT_LE(t1, t2) << "FIRST budget should be <= SECOND budget";
}

TEST(TimeManagerTest, EndgameGetsMore) {
    TimeManager tm;
    SideConfig cfg{1.0f, 0.5f, 1.0f, 1.0f, false};

    int early = tm.get_budget(GamePhase::kOpening, cfg, 5000, 0);
    int late = tm.get_budget(GamePhase::kLate, cfg, 5000, 0);
    int end = tm.get_budget(GamePhase::kEndgame, cfg, 5000, 0);

    EXPECT_LE(early, late);
    EXPECT_LE(late, end);
}

TEST(TimeManagerTest, WinningUsesLess) {
    TimeManager tm;
    SideConfig cfg{1.0f, 0.5f, 1.0f, 1.0f, false};

    int losing = tm.get_budget(GamePhase::kMidgame, cfg, 8000, -50);
    int winning = tm.get_budget(GamePhase::kMidgame, cfg, 8000, 50);
    EXPECT_LE(winning, losing) << "Should spend less when winning";
}

TEST(TimeManagerTest, RespectsHardLimit) {
    TimeManager tm;
    SideConfig cfg{1.0f, 0.5f, 1.0f, 1.0f, false};

    int budget = tm.get_budget(GamePhase::kMidgame, cfg, 300, 0);
    EXPECT_LE(budget, 270) << "Budget should be <= 90% of remaining time";
}
