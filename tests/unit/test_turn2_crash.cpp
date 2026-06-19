#include <gtest/gtest.h>
#include "engine/board.hpp"
#include "common/prefix_sum.hpp"
#include "engine/rect_table.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"

using namespace cordyceps;

// EXACT simulation of testing_tool log:
// FIRST fixes budget 600ms, makes move (6,5)-(6,6)
// SECOND makes move (0,2)-(0,3)
// FIRST's turn 2

TEST(Turn2ExactTest, FirstBotTurn2WithCorrectMoves) {
    RectTable table;
    ASSERT_TRUE(table.load("data.bin"));

    Board board;
    board.current_player = k_player_us;

    // Parse INIT board
    const char* init_line = "INIT 52466443835422223 13254826938785712 87598398564241564 72722154162442227 43968227685821641 27727492381231148 56138649164249527 36281415328768234 62682694189422313 62269566965435457";
    std::istringstream iss(init_line);
    std::string cmd;
    iss >> cmd;
    for (int r = 0; r < k_rows; ++r) {
        unsigned long long row_val;
        iss >> row_val;
        for (int c = k_cols - 1; c >= 0; --c) {
            int digit = static_cast<int>(row_val % 10);
            row_val /= 10;
            board.values[r * k_cols + c] = static_cast<std::int8_t>(digit);
        }
    }
    board.recalc_live_mask();
    board.my_mask = Bitboard::empty();
    board.opp_mask = Bitboard::empty();

    Zobrist zobrist;
    Search search(table, zobrist);
    SideConfig cfg{1.0f, 0.3f, 1.0f, 2.0f, false};

    // --- Turn 1: FIRST moves (6,5)-(6,6) exactly ---
    // (testing tool logs show this move uses 600ms)
    Move first_move{6, 5, 6, 6};
    int s1 = 0;
    for (int r = 6; r <= 6; ++r)
        for (int c = 5; c <= 6; ++c)
            s1 += board.value_at(r, c);
    ASSERT_EQ(s1, 10) << "First move should sum to 10";

    // Search to depth, but we'll use the exact known move
    auto result1 = search.iterative_deepening(board, 600, cfg);
    std::cerr << "Turn 1 search: (" << (int)result1.move.r1 << "," << (int)result1.move.c1
              << ")-(" << (int)result1.move.r2 << "," << (int)result1.move.c2
              << ")" << std::endl;

    // Apply the exact move from the log
    board.apply_move(first_move);
    std::cerr << "After FIRST move: live=" << board.live_count << std::endl;

    // --- Opponent: SECOND moves (0,2)-(0,3) ---
    Move opp_move{0, 2, 0, 3};
    int s_opp = 0;
    for (int r = 0; r <= 0; ++r)
        for (int c = 2; c <= 3; ++c)
            s_opp += board.value_at(r, c);
    ASSERT_EQ(s_opp, 10) << "Opp move should sum to 10 on remaining board";

    board.apply_move(opp_move);
    std::cerr << "After OPP move: live=" << board.live_count << std::endl;

    // --- Turn 2: FIRST searches again with 600ms budget ---
    auto result2 = search.iterative_deepening(board, 600, cfg);
    std::cerr << "Turn 2 search: (" << (int)result2.move.r1 << "," << (int)result2.move.c1
              << ")-(" << (int)result2.move.r2 << "," << (int)result2.move.c2
              << ")" << std::endl;

    ASSERT_FALSE(result2.move.is_pass()) << "Turn 2 should have moves";

    // Verify move is valid
    PrefixSum ps = PrefixSum::from_board(board);
    int sum = ps.sum(result2.move.r1, result2.move.c1, result2.move.r2, result2.move.c2);
    EXPECT_EQ(sum, 10) << "Turn 2 move sum=" << sum;
}
