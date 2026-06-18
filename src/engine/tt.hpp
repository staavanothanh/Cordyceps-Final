#ifndef CORDYCEPS_ENGINE_TT_HPP
#define CORDYCEPS_ENGINE_TT_HPP

#include <cstdint>
#include <vector>
#include "common/types.hpp"

namespace cordyceps {

struct TTEntry {
    enum Flag : std::uint8_t { EMPTY = 0, EXACT = 1, ALPHA = 2, BETA = 3 };

    std::uint64_t key{0};
    Move best_move{k_pass_move};
    int score{0};
    std::int8_t depth{0};
    Flag flag{EMPTY};
};

static_assert(sizeof(TTEntry) <= 32, "TTEntry should be <= 32 bytes");

class TranspositionTable {
public:
    explicit TranspositionTable(int size_power) noexcept;

    // Probe: returns flag. If valid for given depth, fills score and move.
    [[nodiscard]] TTEntry::Flag probe(std::uint64_t key, int depth, int& score, Move& mv) noexcept;

    // Store entry (always replaces)
    void store(std::uint64_t key, int depth, TTEntry::Flag flag, int score, const Move& mv) noexcept;

    // Clear all entries
    void clear() noexcept;

    [[nodiscard]] int size() const noexcept { return static_cast<int>(table_.size()); }

private:
    std::vector<TTEntry> table_;
    int mask_;
};

} // namespace cordyceps

#endif // CORDYCEPS_ENGINE_TT_HPP
