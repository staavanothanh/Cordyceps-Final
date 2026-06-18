#ifndef CORDYCEPS_ENGINE_ZOBRIST_HPP
#define CORDYCEPS_ENGINE_ZOBRIST_HPP

#include <cstdint>
#include <random>
#include "engine/board.hpp"

namespace cordyceps {

class Zobrist {
public:
    Zobrist();

    [[nodiscard]] std::uint64_t compute(const Board& board) const noexcept;

private:
    std::uint64_t z_value_[k_cells][10]{};   // [cell_idx][value 1-9]
    std::uint64_t z_owner_[k_cells][2]{};    // [cell_idx][0=US, 1=OPP]
    std::uint64_t z_player_[2]{};            // [0=US, 1=OPP] — current_player
    std::uint64_t z_passes_[k_cells]{};      // [consecutive_passes]
};

} // namespace cordyceps

#endif // CORDYCEPS_ENGINE_ZOBRIST_HPP
