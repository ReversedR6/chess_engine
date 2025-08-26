#pragma once
#include <cstdint>
using Bitboard = uint64_t;

inline int popcount(Bitboard b){ return __builtin_popcountll(b); }
inline int lsb(Bitboard b){ return __builtin_ctzll(b); }
inline Bitboard poplsb(Bitboard& b){ int s = lsb(b); b &= b - 1; return Bitboard(1ULL) << s; }

constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_H = 0x8080808080808080ULL;
constexpr Bitboard RANK_2 = 0x000000000000FF00ULL;
constexpr Bitboard RANK_7 = 0x00FF000000000000ULL;