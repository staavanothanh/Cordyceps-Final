#ifndef CORDYCEPS_COMMON_PREFIX_SUM_HPP
#define CORDYCEPS_COMMON_PREFIX_SUM_HPP

#include <cstdint>
#include "common/types.hpp"
#include "engine/board.hpp"

namespace cordyceps {

// 2D prefix sum for O(1) rectangle sum queries
// Dimensions: (k_rows+1) x (k_cols+1) with sentinel row/col
class PrefixSum {
public:
    PrefixSum() noexcept;

    // Set a cell value (before build)
    void set(int r, int c, std::int8_t val) noexcept;

    // Build prefix sum table from stored values
    void build() noexcept;

    // O(1) sum of rectangle (r1,c1) to (r2,c2) inclusive
    [[nodiscard]] int sum(int r1, int c1, int r2, int c2) const noexcept;

    // Factory: build from Board values
    [[nodiscard]] static PrefixSum from_board(const Board& board) noexcept;

private:
    std::int8_t raw_[k_rows][k_cols]{};
    int pref_[(k_rows + 1) * (k_cols + 1)]{};
    bool built_{false};
};

} // namespace cordyceps

#endif // CORDYCEPS_COMMON_PREFIX_SUM_HPP
