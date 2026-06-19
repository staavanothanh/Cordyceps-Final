#include <gtest/gtest.h>
#include "engine/board.hpp"
#include "common/prefix_sum.hpp"
#include "engine/rect_table.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"

using namespace cordyceps;

// Same Zobrist, fresh Search on second call — protocol behavior
TEST(Turn2ExactTest, FreshSearchSecondTurn) {
    RectTable table;
    ASSERT_TRUE(table.load("data.bin"));

    Board board;
    board.current_player = k_player_us;
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

    // Protocol creates ONE Zobrist at startup
    Zobrist zobrist;
    SideConfig cfg{1.0f, 0.3f, 1.0f, 2.0f, false};

    // --- Turn 1: FIRST moves ---
    Search search1(table, zobrist);
    auto result1 = search1.iterative_deepening(board, 600, cfg);
    std::cerr << "Turn 1: (" << (int)result1.move.r1 << "," << (int)result1.move.c1
              << ")-(" << (int)result1.move.r2 << "," << (int)result1.move.c2 << ")\n";
    board.apply_move(result1.move);

    // OPP move (0,2)-(0,3)
    Move opp_move{0, 2, 0, 3};
    board.apply_move(opp_move);

    // --- Turn 2: SECOND search (same Zobrist, different Search object) ---
    Search search2(table, zobrist);
    auto result2 = search2.iterative_deepening(board, 600, cfg);
    std::cerr << "Turn 2: (" << (int)result2.move.r1 << "," << (int)result2.move.c1
              << ")-(" << (int)result2.move.r2 << "," << (int)result2.move.c2 << ")\n";
    ASSERT_FALSE(result2.move.is_pass());
    PrefixSum ps = PrefixSum::from_board(board);
    EXPECT_EQ(ps.sum(result2.move.r1, result2.move.c1, result2.move.r2, result2.move.c2), 10);
    board.apply_move(result2.move);
}
