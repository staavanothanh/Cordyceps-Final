#include <gtest/gtest.h>
#include "engine/rect_table.hpp"

using namespace cordyceps;

TEST(RectTableTest, LoadFromFile) {
    RectTable table;
    bool ok = table.load("data.bin");
    ASSERT_TRUE(ok) << "data.bin not found — run gen_geometry first";
    EXPECT_EQ(table.num_rects(), 8415);
}

TEST(RectTableTest, RectIdFormula) {
    RectTable table;
    ASSERT_TRUE(table.load("data.bin"));

    EXPECT_EQ(table.rect_id(0, 0, 0, 0), 0);
    EXPECT_EQ(table.rect_id(0, 0, 0, 16), 16);  // r1=0,r2=0, c từ 0-16 = 17 pairs
    EXPECT_EQ(table.rect_id(0, 0, 1, 0), 153);    // row_pair=1 (r1=0,r2=1), col_pair=0
    EXPECT_EQ(table.rect_id(9, 16, 9, 16), 8414); // last rect
}

TEST(RectTableTest, GetRectInfo) {
    RectTable table;
    ASSERT_TRUE(table.load("data.bin"));

    auto& info = table.get_rect(0);
    EXPECT_EQ(info.r1, 0);
    EXPECT_EQ(info.c1, 0);
    EXPECT_EQ(info.r2, 0);
    EXPECT_EQ(info.c2, 0);
    EXPECT_EQ(info.cell_count, 1);

    auto& last = table.get_rect(8414);
    EXPECT_EQ(last.r1, 9);
    EXPECT_EQ(last.c1, 16);
    EXPECT_EQ(last.r2, 9);
    EXPECT_EQ(last.c2, 16);
}

TEST(RectTableTest, BorderMasksExist) {
    RectTable table;
    ASSERT_TRUE(table.load("data.bin"));

    // Rect (0,0)-(0,16): top row full, top_mask should have 17 bits set
    int id = table.rect_id(0, 0, 0, 16);
    auto& info = table.get_rect(id);
    EXPECT_EQ(info.top_mask.popcount(), 17);
    EXPECT_EQ(info.left_mask.popcount(), 1);  // only (0,0)
    EXPECT_EQ(info.right_mask.popcount(), 1); // only (0,16)

    // Bottom = top for single row
    EXPECT_EQ(info.bottom_mask.popcount(), 17);
}

TEST(RectTableTest, AllRectsHaveValidCount) {
    RectTable table;
    ASSERT_TRUE(table.load("data.bin"));

    for (int i = 0; i < table.num_rects(); ++i) {
        auto& info = table.get_rect(i);
        int expected = (info.r2 - info.r1 + 1) * (info.c2 - info.c1 + 1);
        EXPECT_EQ(info.cell_count, expected)
            << "Rect " << i << " cell_count mismatch";
        EXPECT_GT(info.top_mask.popcount(), 0);
        EXPECT_GT(info.bottom_mask.popcount(), 0);
        EXPECT_GT(info.left_mask.popcount(), 0);
        EXPECT_GT(info.right_mask.popcount(), 0);
    }
}

TEST(RectTableTest, CellMembership) {
    RectTable table;
    ASSERT_TRUE(table.load("data.bin"));

    // Check rect (0,0)-(2,3)
    int id = table.rect_id(0, 0, 2, 3);
    auto cells = table.get_cells(id);
    EXPECT_EQ(cells.size(), 12); // 3 rows × 4 cols

    // Verify first and last cell
    EXPECT_EQ(cells[0], 0);  // (0,0)
    EXPECT_EQ(cells[11], 2 * k_cols + 3); // (2,3)
}
