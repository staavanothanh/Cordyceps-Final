#include <gtest/gtest.h>
#include "engine/board.hpp"
#include "common/prefix_sum.hpp"
#include "engine/rect_table.hpp"
#include "engine/movegen.hpp"
#include <sstream>

using namespace cordyceps;

TEST(ProtocolInitTest, ParseBoardFromInitLine) {
    Board board;
    const char* init_line = "INIT 52466443835422223 13254826938785712 87598398564241564 72722154162442227 43968227685821641 27727492381231148 56138649164249527 36281415328768234 62682694189422313 62269566965435457";

    std::istringstream iss(init_line);
    std::string cmd;
    iss >> cmd; // skip "INIT"

    for (int r = 0; r < k_rows; ++r) {
        unsigned long long row_val;
        ASSERT_TRUE(iss >> row_val) << "Failed to parse row " << r;

        for (int c = k_cols - 1; c >= 0; --c) {
            int digit = static_cast<int>(row_val % 10);
            row_val /= 10;
            int idx = r * k_cols + c;
            board.values[idx] = static_cast<std::int8_t>(digit);
        }
    }

    board.recalc_live_mask();

    // Verify first row
    int expected_row0[] = {5, 2, 4, 6, 6, 4, 4, 3, 8, 3, 5, 4, 2, 2, 2, 2, 3};
    for (int c = 0; c < k_cols; ++c) {
        EXPECT_EQ(board.values[0 * k_cols + c], expected_row0[c])
            << "Row 0, col " << c;
    }

    // Verify last row
    int expected_row9[] = {6, 2, 2, 6, 9, 5, 6, 6, 9, 6, 5, 4, 3, 5, 4, 5, 7};
    for (int c = 0; c < k_cols; ++c) {
        EXPECT_EQ(board.values[9 * k_cols + c], expected_row9[c])
            << "Row 9, col " << c;
    }

    // Verify live count
    EXPECT_EQ(board.live_count, k_cells) << "All 170 cells should have values";

    // Test prefix sum
    PrefixSum ps = PrefixSum::from_board(board);
    
    // Test a known sum=10 rectangle
    // Cell (0,0) = 5 + (1,0) = 3 + (2,0) = 8 = 16, not 10
    // Let me find a real sum=10 rect: cells (0,0) and (0,1) = 5+2=7, not 10
    // Cell (5,0) = 2 + (5,1) = 7 + (5,2) = 7 = 16, not 10
    // Let me check a few random small rects
    
    // Check rect (0,0)-(0,1): 5+2=7
    EXPECT_EQ(ps.sum(0, 0, 0, 1), 7) << "Sum of (0,0)-(0,1) = 5+2 = 7";
    
    // Find at least one valid sum=10 rect
    int found = 0;
    RectTable table;
    ASSERT_TRUE(table.load("data.bin"));
    
    int n = table.num_rects();
    for (int i = 0; i < n && found < 3; ++i) {
        const auto& ri = table.get_rect(i);
        if (ri.r1 < 0 || ri.r2 >= k_rows || ri.c1 < 0 || ri.c2 >= k_cols) continue;
        if (ps.sum(ri.r1, ri.c1, ri.r2, ri.c2) == k_target_sum) {
            // Check inscribed rule
            Bitboard live = board.live_mask;
            if ((ri.top_mask & live).is_empty()) continue;
            if ((ri.bottom_mask & live).is_empty()) continue;
            if ((ri.left_mask & live).is_empty()) continue;
            if ((ri.right_mask & live).is_empty()) continue;
            
            int sum = 0;
            for (int r = ri.r1; r <= ri.r2; ++r)
                for (int c = ri.c1; c <= ri.c2; ++c)
                    sum += board.values[r * k_cols + c];
            
            found++;
            EXPECT_EQ(sum, 10) << "Rect (" << (int)ri.r1 << "," << (int)ri.c1
                              << ")-(" << (int)ri.r2 << "," << (int)ri.c2
                              << ") sum = " << sum << " should be 10";
        }
    }
    EXPECT_GE(found, 1) << "Should find at least one sum=10 rect";
    
    // Use move generation
    auto moves = generate_legal_moves_optimized(board, table);
    EXPECT_GT(moves.size(), 0) << "Should generate at least one legal move";
    if (!moves.empty()) {
        // Verify the first move sums to 10
        const auto& mv = moves[0];
        int sum = 0;
        for (int r = mv.r1; r <= mv.r2; ++r)
            for (int c = mv.c1; c <= mv.c2; ++c)
                sum += board.values[r * k_cols + c];
        EXPECT_EQ(sum, 10) << "Generated move (" << (int)mv.r1 << "," << (int)mv.c1
                          << ")-(" << (int)mv.r2 << "," << (int)mv.c2
                          << ") sum = " << sum;
    }
}
