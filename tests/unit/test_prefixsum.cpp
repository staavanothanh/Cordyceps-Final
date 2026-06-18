#include <gtest/gtest.h>
#include "common/prefix_sum.hpp"

using namespace cordyceps;

TEST(PrefixSumTest, EmptyBoardHasAllZeros) {
    PrefixSum ps;
    for (int r = 0; r < k_rows; ++r) {
        for (int c = 0; c < k_cols; ++c) {
            EXPECT_EQ(ps.sum(0, 0, r, c), 0);
        }
    }
}

TEST(PrefixSumTest, SingleCell) {
    PrefixSum ps;
    ps.set(2, 3, 5);
    ps.build();
    EXPECT_EQ(ps.sum(2, 3, 2, 3), 5);
    EXPECT_EQ(ps.sum(0, 0, 2, 3), 5);
}

TEST(PrefixSumTest, RowSum) {
    PrefixSum ps;
    ps.set(0, 0, 1); ps.set(0, 1, 2); ps.set(0, 2, 3);
    ps.build();
    EXPECT_EQ(ps.sum(0, 0, 0, 0), 1);
    EXPECT_EQ(ps.sum(0, 0, 0, 2), 6);
    EXPECT_EQ(ps.sum(0, 1, 0, 2), 5);
}

TEST(PrefixSumTest, ColumnSum) {
    PrefixSum ps;
    ps.set(0, 0, 1); ps.set(1, 0, 4); ps.set(2, 0, 7);
    ps.build();
    EXPECT_EQ(ps.sum(0, 0, 2, 0), 12);
    EXPECT_EQ(ps.sum(1, 0, 2, 0), 11);
}

TEST(PrefixSumTest, FullRect) {
    PrefixSum ps;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            ps.set(r, c, 1);
    ps.build();
    EXPECT_EQ(ps.sum(0, 0, 2, 2), 9);
    EXPECT_EQ(ps.sum(1, 1, 2, 2), 4);
}

TEST(PrefixSumTest, BuildFromBoard) {
    Board board;
    board.value_at(0, 0) = 1; board.value_at(0, 1) = 2;
    board.value_at(1, 0) = 3; board.value_at(1, 1) = 4;

    PrefixSum ps = PrefixSum::from_board(board);
    EXPECT_EQ(ps.sum(0, 0, 1, 1), 10);
    EXPECT_EQ(ps.sum(0, 0, 0, 1), 3);
}

TEST(PrefixSumTest, BoundaryValues) {
    PrefixSum ps;
    ps.set(9, 16, 9);
    ps.set(0, 0, 1);
    ps.build();
    EXPECT_EQ(ps.sum(0, 0, 0, 0), 1);
    EXPECT_EQ(ps.sum(9, 16, 9, 16), 9);
    EXPECT_EQ(ps.sum(0, 0, 9, 16), 10);
}

TEST(PrefixSumTest, UpdateRequiresRebuild) {
    PrefixSum ps;
    ps.set(0, 0, 5);
    ps.build();
    EXPECT_EQ(ps.sum(0, 0, 0, 0), 5);
    ps.set(0, 0, 10); // change without rebuild
    // sum vẫn là 5 vì chưa rebuild
    EXPECT_EQ(ps.sum(0, 0, 0, 0), 5);
    ps.build();
    EXPECT_EQ(ps.sum(0, 0, 0, 0), 10);
}
