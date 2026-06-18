#include <gtest/gtest.h>
#include "engine/zobrist.hpp"

using namespace cordyceps;

TEST(ZobristTest, EmptyBoardHash) {
    Zobrist z;
    Board board;
    board.recalc_live_mask();
    uint64_t h = z.compute(board);
    EXPECT_NE(h, 0); // hash includes current_player
}

TEST(ZobristTest, HashChangesWithValue) {
    Zobrist z;
    Board board;
    board.value_at(0, 0) = 5;
    board.recalc_live_mask();
    uint64_t h1 = z.compute(board);

    board.value_at(0, 0) = 3;
    board.recalc_live_mask();
    uint64_t h2 = z.compute(board);
    EXPECT_NE(h1, h2);
}

TEST(ZobristTest, HashChangesWithOwner) {
    Zobrist z;
    Board board;
    board.value_at(0, 0) = 5;
    board.current_player = k_player_us;
    board.recalc_live_mask();

    uint64_t h1 = z.compute(board);
    auto undo = board.apply_move({0, 0, 0, 0});
    uint64_t h2 = z.compute(board);
    EXPECT_NE(h1, h2); // cell now owned by US

    board.unmake_move(undo);
    uint64_t h3 = z.compute(board);
    EXPECT_EQ(h1, h3); // restored
}

TEST(ZobristTest, HashChangesWithPass) {
    Zobrist z;
    Board board;
    board.current_player = k_player_us;
    uint64_t h1 = z.compute(board);

    board.apply_move(k_pass_move);
    uint64_t h2 = z.compute(board);
    EXPECT_NE(h1, h2); // player flipped + passes incremented
}

TEST(ZobristTest, IncrementalHashMatchesFull) {
    Zobrist z;
    Board board;
    board.value_at(0, 0) = 3; board.value_at(0, 1) = 4; board.value_at(0, 2) = 3;
    board.value_at(1, 0) = 5; board.value_at(1, 1) = 2; board.value_at(1, 2) = 3;
    board.current_player = k_player_us;
    board.recalc_live_mask();

    uint64_t before = z.compute(board);
    auto undo = board.apply_move({0, 0, 0, 2}); // sum=10
    uint64_t after = z.compute(board);

    EXPECT_NE(before, after);

    // Rebuild from scratch and compare
    Board tmp = board;
    tmp.recalc_live_mask();
    uint64_t rebuilt = z.compute(tmp);
    EXPECT_EQ(after, rebuilt);
}

TEST(ZobristTest, IncrementalUnmakeRestoresHash) {
    Zobrist z;
    Board board;
    for (int c = 0; c < 10; ++c) board.value_at(0, c) = 1;
    board.current_player = k_player_us;
    board.recalc_live_mask();

    uint64_t original = z.compute(board);
    auto undo = board.apply_move({0, 0, 0, 9}); // takes 10 cells sum=10
    uint64_t after_move = z.compute(board);

    board.unmake_move(undo);
    uint64_t restored = z.compute(board);
    EXPECT_EQ(original, restored);
    EXPECT_NE(original, after_move);
}

TEST(ZobristTest, DifferentBoardStateDifferentHash) {
    Zobrist z;
    Board a, b;
    a.value_at(0, 0) = 1; a.recalc_live_mask();
    b.value_at(0, 0) = 2; b.recalc_live_mask();

    EXPECT_NE(z.compute(a), z.compute(b));

    b.value_at(0, 0) = 1;
    b.value_at(0, 1) = 1;
    b.recalc_live_mask();
    EXPECT_NE(z.compute(a), z.compute(b));
}
