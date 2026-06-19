#include "engine/rect_table.hpp"
#include <cstdio>

namespace cordyceps {

bool RectTable::load(const char* filename) noexcept {
    FILE* f = std::fopen(filename, "rb");
    if (!f) return false;

    std::uint32_t magic, num_rects, cell_table_size, checksum;
    if (std::fread(&magic, 4, 1, f) != 1 || magic != 0x43524459) {
        std::fclose(f); return false;
    }
    { auto _ = std::fread(&num_rects, 4, 1, f); (void)_; }
    { auto _ = std::fread(&cell_table_size, 4, 1, f); (void)_; }
    { auto _ = std::fread(&checksum, 4, 1, f); (void)_; }

    rects_.resize(num_rects);

    // Read packed records (same layout as gen_geometry RectRecord)
    #pragma pack(push, 1)
    struct PackedRect {
        std::int8_t r1, c1, r2, c2;
        std::uint16_t cell_count, cell_offset;
        std::uint64_t top_lo, top_mid, top_hi;
        std::uint64_t bottom_lo, bottom_mid, bottom_hi;
        std::uint64_t left_lo, left_mid, left_hi;
        std::uint64_t right_lo, right_mid, right_hi;
    };
    #pragma pack(pop)
    static_assert(sizeof(PackedRect) == 104, "PackedRect must be 104 bytes");

    for (std::uint32_t i = 0; i < num_rects; ++i) {
        PackedRect pr;
        { auto _ = std::fread(&pr, sizeof(PackedRect), 1, f); (void)_; }
        auto& ri = rects_[i];
        ri.r1 = pr.r1; ri.c1 = pr.c1; ri.r2 = pr.r2; ri.c2 = pr.c2;
        ri.cell_count = pr.cell_count;
        ri.cell_offset = pr.cell_offset;
        ri.top_mask    = {pr.top_lo, pr.top_mid, pr.top_hi};
        ri.bottom_mask = {pr.bottom_lo, pr.bottom_mid, pr.bottom_hi};
        ri.left_mask   = {pr.left_lo, pr.left_mid, pr.left_hi};
        ri.right_mask  = {pr.right_lo, pr.right_mid, pr.right_hi};
    }

    cell_table_.resize(cell_table_size);
    { auto _ = std::fread(cell_table_.data(), 1, cell_table_size, f); (void)_; }
    std::fclose(f);
    return true;
}

int RectTable::rect_id(int r1, int c1, int r2, int c2) const noexcept {
    constexpr int k_num_col_pairs = k_cols * (k_cols + 1) / 2; // 153

    int row_offset = r1 * k_rows - r1 * (r1 - 1) / 2;
    int row_pair = row_offset + (r2 - r1);

    int col_offset = c1 * k_cols - c1 * (c1 - 1) / 2;
    int col_pair = col_offset + (c2 - c1);

    return row_pair * k_num_col_pairs + col_pair;
}

std::span<const std::uint8_t> RectTable::get_cells(int id) const noexcept {
    const auto& info = rects_[id];
    return {cell_table_.data() + info.cell_offset, info.cell_count};
}

} // namespace cordyceps
