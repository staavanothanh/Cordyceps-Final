#include "common/bitboard.hpp"
#include <bit>

namespace cordyceps {

int Bitboard::popcount() const noexcept {
    return std::popcount(lo) + std::popcount(mid) + std::popcount(hi);
}

void Bitboard::set(int idx) noexcept {
    if (idx < 64)       lo |= (1ULL << idx);
    else if (idx < 128) mid |= (1ULL << (idx - 64));
    else                hi  |= (1ULL << (idx - 128));
}

void Bitboard::clear(int idx) noexcept {
    if (idx < 64)       lo &= ~(1ULL << idx);
    else if (idx < 128) mid &= ~(1ULL << (idx - 64));
    else                hi  &= ~(1ULL << (idx - 128));
}

bool Bitboard::test(int idx) const noexcept {
    if (idx < 64)       return (lo >> idx) & 1ULL;
    else if (idx < 128) return (mid >> (idx - 64)) & 1ULL;
    else                return (hi >> (idx - 128)) & 1ULL;
}

Bitboard& Bitboard::operator&=(const Bitboard& other) noexcept {
    lo &= other.lo; mid &= other.mid; hi &= other.hi;
    return *this;
}

Bitboard& Bitboard::operator|=(const Bitboard& other) noexcept {
    lo |= other.lo; mid |= other.mid; hi |= other.hi;
    return *this;
}

Bitboard& Bitboard::operator^=(const Bitboard& other) noexcept {
    lo ^= other.lo; mid ^= other.mid; hi ^= other.hi;
    return *this;
}

bool Bitboard::is_empty() const noexcept {
    return (lo | mid | hi) == 0;
}

} // namespace cordyceps
