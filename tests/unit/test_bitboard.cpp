#include <gtest/gtest.h>
#include "common/bitboard.hpp"

using namespace cordyceps;

// === Empty / Initial State ===

TEST(BitboardTest, EmptyBitboardHasZeroPopcount) {
    auto bb = Bitboard::empty();
    EXPECT_EQ(bb.popcount(), 0);
    EXPECT_TRUE(bb.is_empty());
}

// === Set / Test / Clear ===

TEST(BitboardTest, SetAndTestSingleBit) {
    auto bb = Bitboard::empty();
    bb.set(0);
    EXPECT_TRUE(bb.test(0));
    EXPECT_FALSE(bb.test(1));
    EXPECT_EQ(bb.popcount(), 1);
}

TEST(BitboardTest, SetAndClear) {
    auto bb = Bitboard::empty();
    bb.set(42);
    EXPECT_TRUE(bb.test(42));
    bb.clear(42);
    EXPECT_FALSE(bb.test(42));
    EXPECT_EQ(bb.popcount(), 0);
}

TEST(BitboardTest, SetBoundaryBits) {
    auto bb = Bitboard::empty();
    bb.set(63);   // cuối lo
    EXPECT_TRUE(bb.test(63));
    bb.set(64);   // đầu mid
    EXPECT_TRUE(bb.test(64));
    bb.set(127);  // cuối mid
    EXPECT_TRUE(bb.test(127));
    bb.set(128);  // đầu hi
    EXPECT_TRUE(bb.test(128));
    bb.set(169);  // cell cuối cùng
    EXPECT_TRUE(bb.test(169));
    EXPECT_EQ(bb.popcount(), 5);
}

TEST(BitboardTest, SetAllCells) {
    auto bb = Bitboard::empty();
    for (int i = 0; i < k_cells; ++i) {
        bb.set(i);
    }
    EXPECT_EQ(bb.popcount(), k_cells);
    for (int i = 0; i < k_cells; ++i) {
        EXPECT_TRUE(bb.test(i));
    }
}

// === Popcount ===

TEST(BitboardTest, PopcountAfterMultipleSets) {
    auto bb = Bitboard::empty();
    for (int i : {0, 10, 63, 64, 100, 127, 128, 150, 169}) {
        bb.set(i);
    }
    EXPECT_EQ(bb.popcount(), 9);
}

// === Operators ===

TEST(BitboardTest, AndOperator) {
    auto a = Bitboard::empty(); a.set(0); a.set(10);
    auto b = Bitboard::empty(); b.set(10); b.set(20);
    auto c = a & b;
    EXPECT_TRUE(c.test(10));
    EXPECT_FALSE(c.test(0));
    EXPECT_FALSE(c.test(20));
    EXPECT_EQ(c.popcount(), 1);
}

TEST(BitboardTest, OrOperator) {
    auto a = Bitboard::empty(); a.set(0);
    auto b = Bitboard::empty(); b.set(10);
    auto c = a | b;
    EXPECT_TRUE(c.test(0));
    EXPECT_TRUE(c.test(10));
    EXPECT_EQ(c.popcount(), 2);
}

TEST(BitboardTest, XorOperator) {
    auto a = Bitboard::empty(); a.set(0); a.set(10);
    auto b = Bitboard::empty(); b.set(10); b.set(20);
    auto c = a ^ b;
    EXPECT_TRUE(c.test(0));
    EXPECT_FALSE(c.test(10)); // XOR cancels shared bit
    EXPECT_TRUE(c.test(20));
    EXPECT_EQ(c.popcount(), 2);
}

TEST(BitboardTest, CompoundAssignment) {
    auto bb = Bitboard::empty();
    bb |= Bitboard::empty();
    EXPECT_EQ(bb.popcount(), 0);

    auto a = Bitboard::empty(); a.set(5);
    bb ^= a;
    EXPECT_TRUE(bb.test(5));
    bb ^= a;
    EXPECT_FALSE(bb.test(5)); // XOR twice = identity

    auto b = Bitboard::empty(); b.set(3);
    bb &= b;
    EXPECT_EQ(bb.popcount(), 0);
}

// === IsEmpty ===

TEST(BitboardTest, IsEmptyAfterClearAll) {
    auto bb = Bitboard::empty();
    for (int i = 0; i < k_cells; ++i) bb.set(i);
    EXPECT_FALSE(bb.is_empty());
    for (int i = 0; i < k_cells; ++i) bb.clear(i);
    EXPECT_TRUE(bb.is_empty());
}
