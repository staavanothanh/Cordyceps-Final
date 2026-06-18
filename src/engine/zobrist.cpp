#include "engine/zobrist.hpp"

namespace cordyceps {

static std::uint64_t random_u64() {
    static std::mt19937_64 rng(123456789);
    return rng();
}

Zobrist::Zobrist() {
    for (int i = 0; i < k_cells; ++i) {
        for (int v = 0; v < 10; ++v) {
            z_value_[i][v] = random_u64();
        }
        z_owner_[i][0] = random_u64(); // US
        z_owner_[i][1] = random_u64(); // OPP
    }
    z_player_[0] = random_u64();
    z_player_[1] = random_u64();
    for (int i = 0; i < k_cells; ++i) {
        z_passes_[i] = random_u64();
    }
}

std::uint64_t Zobrist::compute(const Board& board) const noexcept {
    std::uint64_t h = 0;

    for (int i = 0; i < k_cells; ++i) {
        std::int8_t v = board.values[i];
        std::int8_t o = board.owners[i];

        if (v > 0) {
            h ^= z_value_[i][v];
        }
        if (o == k_player_us) {
            h ^= z_owner_[i][0];
        } else if (o == k_player_opp) {
            h ^= z_owner_[i][1];
        }
    }

    // Current player
    if (board.current_player == k_player_us) {
        h ^= z_player_[0];
    } else if (board.current_player == k_player_opp) {
        h ^= z_player_[1];
    }

    h ^= z_passes_[board.consecutive_passes];
    return h;
}

} // namespace cordyceps
