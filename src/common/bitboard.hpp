#ifndef CORDYCEPS_COMMON_BITBOARD_HPP
#define CORDYCEPS_COMMON_BITBOARD_HPP

#include <cstdint>
#include "types.hpp"

namespace cordyceps {

// 3 × uint64_t covers 170 cells (uses 170/192 bits)
struct Bitboard {
    std::uint64_t lo;   // cells 0-63
    std::uint64_t mid;  // cells 64-127
    std::uint64_t hi;   // cells 128-169 (42 bits used)

    static constexpr Bitboard empty() noexcept { return {0, 0, 0}; }

    [[nodiscard]] int popcount() const noexcept;

    void set(int idx) noexcept;
    void clear(int idx) noexcept;
    [[nodiscard]] bool test(int idx) const noexcept;

    Bitboard& operator&=(const Bitboard& other) noexcept;
    Bitboard& operator|=(const Bitboard& other) noexcept;
    Bitboard& operator^=(const Bitboard& other) noexcept;

    friend Bitboard operator&(Bitboard a, const Bitboard& b) noexcept { a &= b; return a; }
    friend Bitboard operator|(Bitboard a, const Bitboard& b) noexcept { a |= b; return a; }
    friend Bitboard operator^(Bitboard a, const Bitboard& b) noexcept { a ^= b; return a; }

    [[nodiscard]] bool is_empty() const noexcept;
};

} // namespace cordyceps

#endif // CORDYCEPS_COMMON_BITBOARD_HPP
