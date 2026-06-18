#include "common/prefix_sum.hpp"

namespace cordyceps {

PrefixSum::PrefixSum() noexcept {
    for (int r = 0; r < k_rows; ++r)
        for (int c = 0; c < k_cols; ++c)
            raw_[r][c] = 0;
}

void PrefixSum::set(int r, int c, std::int8_t val) noexcept {
    raw_[r][c] = val;
    built_ = false;
}

void PrefixSum::build() noexcept {
    for (int r = 0; r <= k_rows; ++r) {
        for (int c = 0; c <= k_cols; ++c) {
            if (r == 0 || c == 0) {
                pref_[r * (k_cols + 1) + c] = 0;
            } else {
                pref_[r * (k_cols + 1) + c] = 
                    pref_[(r - 1) * (k_cols + 1) + c]
                    + pref_[r * (k_cols + 1) + (c - 1)]
                    - pref_[(r - 1) * (k_cols + 1) + (c - 1)]
                    + raw_[r - 1][c - 1];
            }
        }
    }
    built_ = true;
}

int PrefixSum::sum(int r1, int c1, int r2, int c2) const noexcept {
    const int R1 = r1, C1 = c1, R2 = r2 + 1, C2 = c2 + 1;
    return pref_[R2 * (k_cols + 1) + C2]
         - pref_[R1 * (k_cols + 1) + C2]
         - pref_[R2 * (k_cols + 1) + C1]
         + pref_[R1 * (k_cols + 1) + C1];
}

PrefixSum PrefixSum::from_board(const Board& board) noexcept {
    PrefixSum ps;
    for (int r = 0; r < k_rows; ++r)
        for (int c = 0; c < k_cols; ++c)
            ps.set(r, c, board.value_at(r, c));
    ps.build();
    return ps;
}

} // namespace cordyceps
