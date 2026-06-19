#include "engine/tt.hpp"

namespace cordyceps {

TranspositionTable::TranspositionTable(int size_power) noexcept {
    int sz = 1 << size_power;
    table_.assign(sz, {});
    mask_ = sz - 1;
}

TTEntry::Flag TranspositionTable::probe(std::uint64_t key, int depth, int& score, Move& mv) noexcept {
    int idx = key & mask_;
    const auto& entry = table_[idx];

    if (entry.key != key) return TTEntry::EMPTY;
    if (entry.depth < depth) return TTEntry::EMPTY;

    score = entry.score;
    mv = entry.best_move;
    return entry.flag;
}

void TranspositionTable::store(std::uint64_t key, int depth, TTEntry::Flag flag, int score, const Move& mv) noexcept {
    int idx = key & mask_;
    auto& entry = table_[idx];

    // Depth-preferred: keep deeper entry from a different position
    // Always replace same position
    if (entry.flag != TTEntry::EMPTY && entry.key != key && entry.depth > depth) {
        return;
    }

    entry.key = key;
    entry.depth = static_cast<std::int8_t>(depth);
    entry.flag = flag;
    entry.score = score;
    entry.best_move = mv;
}

void TranspositionTable::clear() noexcept {
    for (auto& e : table_) {
        e.key = 0;
        e.flag = TTEntry::EMPTY;
    }
}

} // namespace cordyceps
