#include <gtest/gtest.h>
#include "engine/timeman.hpp"
#include <iostream>

using namespace cordyceps;

// Verify time budget matches log data from top engines

TEST(TimeBudgetBenchmark, FirstVsSecondPhaseBudget) {
    TimeManager tm;
    SideConfig first{1.0f, 0.3f, 1.0f, 2.0f, false};
    SideConfig second{1.5f, 0.7f, 1.0f, 1.0f, true};

    struct TestCase { const char* label; int live; int rem; int margin; };
    TestCase cases[] = {
        {"Opening (live=80)",  80, 10000, 0},
        {"Opening (live=40)",  40,  9000, 0},
        {"Midgame (live=30)",  30,  8000, 0},
        {"Midgame (live=22)",  22,  7000, 0},
        {"Late (live=16)",     16,  6000, 0},
        {"Late (live=13)",     13,  3000, 0},
        {"Endgame (live=8)",    8,  1500, 0},
        {"Endgame (live=4)",    4,   800, 0},
    };

    std::cout << "\n=== TIME BUDGET: FIRST vs SECOND (log-data formula) ===" << std::endl;
    std::cout << "Phase            |rem | FIRST(ms) | SECOND(ms) | Ratio" << std::endl;
    std::cout << "-----------------+----+-----------+------------+-------" << std::endl;

    float sum_ratio = 0.0f;
    int count = 0;
    for (auto& c : cases) {
        int b1 = tm.get_budget(c.live, first, c.rem, c.margin);
        int b2 = tm.get_budget(c.live, second, c.rem, c.margin);
        float ratio = (b1 > 0) ? static_cast<float>(b2) / b1 : 0;

        printf("%-17s | %3d | %-9d | %-10d | %.2f\n", c.label, c.rem, b1, b2, ratio);
        sum_ratio += ratio;
        count++;
    }
    printf("-----------------+----+-----------+------------+-------\n");
    printf("AVG Ratio: %.2f\n", sum_ratio / count);

    float avg = sum_ratio / count;
    EXPECT_GE(avg, 1.3f) << "Average SECOND/FIRST ratio >= 1.3";
    EXPECT_LE(avg, 2.5f) << "Average SECOND/FIRST ratio <= 2.5";
}

TEST(TimeBudgetBenchmark, GameSimulationLogCheck) {
    TimeManager tm;

    struct Side { const char* name; SideConfig cfg; };
    Side sides[] = {
        {"FIRST",  {1.0f, 0.3f, 1.0f, 2.0f, false}},
        {"SECOND", {1.5f, 0.7f, 1.0f, 1.0f, true}},
    };

    for (auto& s : sides) {
        int remaining = 10000;
        int live = 140;
        int margin = 0;
        int total_spent = 0, moves = 0;

        // Simulate ~20 moves
        for (int m = 1; m <= 25 && remaining > 500; ++m) {
            int budget = tm.get_budget(live, s.cfg, remaining, margin);
            total_spent += budget;
            moves++;
            remaining -= budget;
            if (remaining < 100) remaining = 100; // safety

            // Simulate mushrooms eaten
            live -= 3;
            if (m > 8) live -= 2; // accelerate
            if (m > 15) live -= 3;
            if (live < 2) live = 2;

            // Simulate margin
            if (margin >= 0) margin += 2;
            else margin -= 1;
            if (margin > 50) margin = 50;
            if (margin < -50) margin = -50;
        }

        float avg = moves > 0 ? static_cast<float>(total_spent) / moves : 0;
        float pct = static_cast<float>(total_spent) / 10000 * 100;

        std::cout << "\n=== GAME SIM: " << s.name << " ===" << std::endl;
        printf("  %d moves, %dms total, avg %.0fms/move, %.0f%% of pool\n",
               moves, total_spent, avg, pct);

        // Log data: winning FIRST avg 9.22%/move across all phases
        // So for 20 moves: ~1844ms total, ~92ms avg
        // But budget % of remaining means later moves spend less
        // Expected: avg somewhere in 80-280ms range
        EXPECT_GE(avg, 60.0f) << s.name << " avg >= 60ms";
        EXPECT_LE(avg, 400.0f) << s.name << " avg <= 400ms";
        EXPECT_LE(pct, 90.0f) << s.name << " total % <= 90%";
    }
}
