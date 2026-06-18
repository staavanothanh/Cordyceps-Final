#ifndef CORDYCEPS_ENGINE_SEARCH_HPP
#define CORDYCEPS_ENGINE_SEARCH_HPP

#include "common/types.hpp"
#include "engine/board.hpp"
#include "engine/rect_table.hpp"
#include "engine/movegen.hpp"

namespace cordyceps {

struct SearchResult {
    Move move;
    int eval;
};

class Search {
public:
    explicit Search(const RectTable& table) noexcept : table_(table) {}

    // 1-ply greedy: try all legal moves, pick best eval from player's perspective
    [[nodiscard]] SearchResult simple_search(const Board& board, const SideConfig& config) noexcept;

private:
    const RectTable& table_;
};

} // namespace cordyceps

#endif // CORDYCEPS_ENGINE_SEARCH_HPP
