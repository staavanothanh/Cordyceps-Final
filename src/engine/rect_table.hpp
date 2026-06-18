#ifndef CORDYCEPS_ENGINE_RECT_TABLE_HPP
#define CORDYCEPS_ENGINE_RECT_TABLE_HPP

#include <cstdint>
#include <vector>
#include <span>
#include "common/types.hpp"
#include "common/bitboard.hpp"

namespace cordyceps {

struct RectInfo {
    std::int8_t r1, c1, r2, c2;
    std::uint16_t cell_count;
    std::uint16_t cell_offset;
    Bitboard top_mask;
    Bitboard bottom_mask;
    Bitboard left_mask;
    Bitboard right_mask;
};

static_assert(sizeof(RectInfo) == 4 + 2 + 2 + 24 + 24 + 24 + 24, "RectInfo size mismatch");

class RectTable {
public:
    RectTable() noexcept = default;

    [[nodiscard]] bool load(const char* filename) noexcept;

    [[nodiscard]] int num_rects() const noexcept { return static_cast<int>(rects_.size()); }

    // O(1) rectangle ID from coordinates
    [[nodiscard]] int rect_id(int r1, int c1, int r2, int c2) const noexcept;

    // O(1) rect info lookup
    [[nodiscard]] const RectInfo& get_rect(int id) const noexcept { return rects_[id]; }

    // O(1) cell membership (span of indices in cell_table_)
    [[nodiscard]] std::span<const std::uint8_t> get_cells(int id) const noexcept;

private:
    std::vector<RectInfo> rects_;
    std::vector<std::uint8_t> cell_table_;
};

} // namespace cordyceps

#endif // CORDYCEPS_ENGINE_RECT_TABLE_HPP
