#include "engine/movegen.hpp"
#include "engine/rect_table.hpp"
#include "common/prefix_sum.hpp"

namespace cordyceps {

int rect_sum(const Board& board, int r1, int c1, int r2, int c2) noexcept {
    int sum = 0;
    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c) {
            sum += board.value_at(r, c);
        }
    }
    return sum;
}

bool check_inscribed(const Board& board, int r1, int c1, int r2, int c2) noexcept {
    // Top edge
    bool ok = false;
    for (int c = c1; c <= c2 && !ok; ++c) {
        if (board.value_at(r1, c) > 0) ok = true;
    }
    if (!ok) return false;

    ok = false;
    for (int c = c1; c <= c2 && !ok; ++c) {
        if (board.value_at(r2, c) > 0) ok = true;
    }
    if (!ok) return false;

    ok = false;
    for (int r = r1; r <= r2 && !ok; ++r) {
        if (board.value_at(r, c1) > 0) ok = true;
    }
    if (!ok) return false;

    ok = false;
    for (int r = r1; r <= r2 && !ok; ++r) {
        if (board.value_at(r, c2) > 0) ok = true;
    }
    return ok;
}

std::vector<Move> generate_legal_moves(const Board& board) {
    std::vector<Move> moves;
    moves.reserve(256);

    for (int r1 = 0; r1 < k_rows; ++r1) {
        for (int r2 = r1; r2 < k_rows; ++r2) {
            int col_sums[k_cols]{};
            for (int c = 0; c < k_cols; ++c) {
                for (int r = r1; r <= r2; ++r) {
                    col_sums[c] += board.value_at(r, c);
                }
            }

            for (int c1 = 0; c1 < k_cols; ++c1) {
                int wsum = 0;
                for (int c2 = c1; c2 < k_cols; ++c2) {
                    wsum += col_sums[c2];
                    if (wsum > k_target_sum) break;

                    if (wsum == k_target_sum && check_inscribed(board, r1, c1, r2, c2)) {
                        moves.push_back({
                            static_cast<std::int8_t>(r1),
                            static_cast<std::int8_t>(c1),
                            static_cast<std::int8_t>(r2),
                            static_cast<std::int8_t>(c2)
                        });
                    }
                }
            }
        }
    }

    return moves;
}

std::vector<Move> generate_legal_moves_optimized(const Board& board, const RectTable& table) {
    PrefixSum ps = PrefixSum::from_board(board);
    std::vector<Move> moves;
    moves.reserve(256);

    int n = table.num_rects();
    for (int i = 0; i < n; ++i) {
        const auto& ri = table.get_rect(i);

        // Fast sum check with prefix sum
        if (ps.sum(ri.r1, ri.c1, ri.r2, ri.c2) != k_target_sum) continue;

        // Fast inscribed check with border masks
        Bitboard live = board.live_mask;
        if ((ri.top_mask & live).is_empty()) continue;
        if ((ri.bottom_mask & live).is_empty()) continue;
        if ((ri.left_mask & live).is_empty()) continue;
        if ((ri.right_mask & live).is_empty()) continue;

        moves.push_back({ri.r1, ri.c1, ri.r2, ri.c2});
    }

    return moves;
}

} // namespace cordyceps
