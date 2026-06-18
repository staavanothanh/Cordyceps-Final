#ifndef CORDYCEPS_ENGINE_MOVEGEN_HPP
#define CORDYCEPS_ENGINE_MOVEGEN_HPP

#include <vector>
#include "common/types.hpp"
#include "engine/board.hpp"

namespace cordyceps {

class RectTable;

// O(1) rectangle sum via 2D prefix sum
[[nodiscard]] int rect_sum(const Board& board, int r1, int c1, int r2, int c2) noexcept;

// Check inscribed rule: all 4 edges must touch at least one remaining mushroom
[[nodiscard]] bool check_inscribed(const Board& board, int r1, int c1, int r2, int c2) noexcept;

// Generate all valid (sum=10, inscribed) legal moves (brute-force)
[[nodiscard]] std::vector<Move> generate_legal_moves(const Board& board);

// Optimized: uses RectTable + PrefixSum for fast generation
[[nodiscard]] std::vector<Move> generate_legal_moves_optimized(const Board& board, const RectTable& table);

} // namespace cordyceps

#endif // CORDYCEPS_ENGINE_MOVEGEN_HPP
