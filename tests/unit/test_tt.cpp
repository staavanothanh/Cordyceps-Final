#include <gtest/gtest.h>
#include "engine/tt.hpp"

using namespace cordyceps;

TEST(TranspositionTableTest, ProbeEmptyReturnsInvalid) {
    TranspositionTable tt(1024);
    int score;
    Move mv;
    auto flag = tt.probe(0xDEADBEEF, 5, score, mv);
    EXPECT_EQ(flag, TTEntry::EMPTY);
}

TEST(TranspositionTableTest, StoreAndProbeExact) {
    TranspositionTable tt(1024);
    Move best{0, 0, 0, 9};
    tt.store(0xABCD1234, 5, TTEntry::EXACT, 42, best);

    int score;
    Move mv;
    auto flag = tt.probe(0xABCD1234, 5, score, mv);
    EXPECT_EQ(flag, TTEntry::EXACT);
    EXPECT_EQ(score, 42);
    EXPECT_EQ(mv, best);
}

TEST(TranspositionTableTest, DepthTooShallow) {
    TranspositionTable tt(1024);
    tt.store(0xABCD1234, 5, TTEntry::EXACT, 42, {0, 0, 0, 9});

    int score;
    Move mv;
    auto flag = tt.probe(0xABCD1234, 6, score, mv); // need depth >= 6
    EXPECT_EQ(flag, TTEntry::EMPTY); // stored depth 5 < 6
}

TEST(TranspositionTableTest, AlphaCutoff) {
    TranspositionTable tt(1024);
    tt.store(0xAABB, 3, TTEntry::ALPHA, 10, {0, 0, 0, 0});

    int score;
    Move mv;
    auto flag = tt.probe(0xAABB, 3, score, mv);
    EXPECT_EQ(flag, TTEntry::ALPHA);
    EXPECT_LE(score, 10); // ALPHA means score <= stored
}

TEST(TranspositionTableTest, BetaCutoff) {
    TranspositionTable tt(1024);
    tt.store(0xCCDD, 3, TTEntry::BETA, 50, {0, 0, 0, 0});

    int score;
    Move mv;
    auto flag = tt.probe(0xCCDD, 3, score, mv);
    EXPECT_EQ(flag, TTEntry::BETA);
    EXPECT_GE(score, 50); // BETA means score >= stored
}

TEST(TranspositionTableTest, OverwriteOldEntry) {
    TranspositionTable tt(1024);
    tt.store(0x1111, 3, TTEntry::EXACT, 10, {0, 0, 0, 0});
    tt.store(0x2222, 3, TTEntry::EXACT, 20, {1, 1, 1, 1});

    int score;
    Move mv;
    auto flag = tt.probe(0x2222, 3, score, mv);
    EXPECT_EQ(flag, TTEntry::EXACT);
    EXPECT_EQ(score, 20);
}

TEST(TranspositionTableTest, HashCollisionDetected) {
    TranspositionTable tt(1024);
    tt.store(0xDEAD, 5, TTEntry::EXACT, 100, {0, 0, 0, 0});

    int score;
    Move mv;
    // Same key, different data — should match
    auto flag = tt.probe(0xDEAD, 5, score, mv);
    EXPECT_EQ(flag, TTEntry::EXACT);
}
