#include <gtest/gtest.h>
#include "common/prefix_sum.hpp"
#include "engine/board.hpp"
#include "engine/rect_table.hpp"
#include "engine/movegen.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"
#include <sstream>

using namespace cordyceps;

// Verify that INIT parsing + search works end-to-end
// Board is parsed correctly, search returns a valid SUM=10 move

TEST(ProtocolEndToEndTest, InitAndSearchProduceValidMove) {
    RectTable table;
    ASSERT_TRUE(table.load("data.bin"));

    // Create board (same as handle_init)
    Board board;
    board.current_player = k_player_us;

    const char* init_line = "INIT 52466443835422223 13254826938785712 87598398564241564 72722154162442227 43968227685821641 27727492381231148 56138649164249527 36281415328768234 62682694189422313 62269566965435457";
    std::istringstream iss(init_line);
    std::string cmd;
    iss >> cmd;
    for (int r = 0; r < k_rows; ++r) {
        unsigned long long row_val;
        ASSERT_TRUE(iss >> row_val) << "Parse row " << r;
        for (int c = k_cols - 1; c >= 0; --c) {
            int digit = static_cast<int>(row_val % 10);
            row_val /= 10;
            board.values[r * k_cols + c] = static_cast<std::int8_t>(digit);
        }
    }
    board.recalc_live_mask();

    // SNAPSHOT board before search
    Board board_copy = board;

    // Get legal moves from FRESH board
    auto moves = generate_legal_moves_optimized(board, table);
    ASSERT_GT(moves.size(), 0) << "Should have legal moves on initial board";

    // SHOW first legal moves
    std::cerr << "First 5 legal moves:" << std::endl;
    for (size_t i = 0; i < std::min(moves.size(), size_t(5)); ++i) {
        const auto& mv = moves[i];
        int sum = 0;
        for (int r = mv.r1; r <= mv.r2; ++r)
            for (int c = mv.c1; c <= mv.c2; ++c)
                sum += board_copy.value_at(r, c);
        std::cerr << "  (" << (int)mv.r1 << "," << (int)mv.c1
                  << ")-(" << (int)mv.r2 << "," << (int)mv.c2
                  << ") sum=" << sum << " area=" << ((int)mv.r2 - mv.r1 + 1)*((int)mv.c2 - mv.c1 + 1)
                  << std::endl;
    }

    // Verify all returned moves sum to 10 on the FRESH board
    for (size_t i = 0; i < std::min(moves.size(), size_t(10)); ++i) {
        const auto& mv = moves[i];
        int sum = 0;
        for (int r = mv.r1; r <= mv.r2; ++r)
            for (int c = mv.c1; c <= mv.c2; ++c)
                sum += board_copy.value_at(r, c);
        EXPECT_EQ(sum, 10) << "Legal move " << i << " sum=" << sum;
    }

    // Run search
    Zobrist zobrist;
    Search search(table, zobrist);
    SideConfig config{1.0f, 0.3f, 1.0f, 2.0f, false};
    auto result = search.iterative_deepening(board, 8000, config);

    // Verify result is valid against the BOARD COPY (pre-search)
    EXPECT_FALSE(result.move.is_pass()) << "Should not pass on full board";

    if (!result.move.is_pass()) {
        // Compute sum against board copy (pre-search state)
        int sum = 0;
        for (int r = result.move.r1; r <= result.move.r2; ++r)
            for (int c = result.move.c1; c <= result.move.c2; ++c)
                sum += board_copy.value_at(r, c);

        EXPECT_EQ(sum, 10) << "Search returned sum=" << sum << " not 10";

        // Verify it's in legal move list
        bool found = false;
        for (const auto& mv : moves) {
            if (mv.r1 == result.move.r1 && mv.c1 == result.move.c1 &&
                mv.r2 == result.move.r2 && mv.c2 == result.move.c2) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Move should be in legal move list";
    }
}
