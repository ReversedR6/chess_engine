#include "eval.h"
#include "bitboard.h"

// File: eval.cpp
// Purpose: Static evaluation of a chess position.
// Score is from the side to move perspective. Positive is good for side to move.
// Components:
//   1) Material balance
//   2) Piece square tables (small positional nudges)

// Flip ranks for Black when indexing PSTs
static inline int mirror64(int sq) {
  return sq ^ 56;
}

// -----------------------------
// Piece square tables, White view
// Keep numbers small. They guide, they do not override material.
// -----------------------------
static const int PST_PAWN[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
  10, 10, 10, 10, 10, 10, 10, 10,
   2,  2,  4,  6,  6,  4,  2,  2,
   1,  1,  2,  5,  5,  2,  1,  1,
   0,  0,  1,  4,  4,  1,  0,  0,
   1, -1,  0,  2,  2,  0, -1,  1,
   1,  2,  2, -2, -2,  2,  2,  1,
   0,  0,  0,  0,  0,  0,  0,  0
};

static const int PST_KNIGHT[64] = {
  -5, -4, -3, -3, -3, -3, -4, -5,
  -4, -2,  0,  0,  0,  0, -2, -4,
  -3,  0,  1,  2,  2,  1,  0, -3,
  -3,  1,  2,  3,  3,  2,  1, -3,
  -3,  0,  2,  3,  3,  2,  0, -3,
  -3,  1,  1,  2,  2,  1,  1, -3,
  -4, -2,  0,  1,  1,  0, -2, -4,
  -5, -4, -3, -3, -3, -3, -4, -5
};

static const int PST_BISHOP[64] = {
  -2, -1, -1, -1, -1, -1, -1, -2,
  -1,  0,  0,  0,  0,  0,  0, -1,
  -1,  0,  1,  1,  1,  1,  0, -1,
  -1,  1,  1,  2,  2,  1,  1, -1,
  -1,  0,  1,  2,  2,  1,  0, -1,
  -1,  1,  1,  1,  1,  1,  1, -1,
  -1,  0,  0,  0,  0,  0,  0, -1,
  -2, -1, -1, -1, -1, -1, -1, -2
};

static const int PST_ROOK[64] = {
   0,  0,  1,  2,  2,  1,  0,  0,
  -1,  0,  0,  0,  0,  0,  0, -1,
  -1,  0,  0,  0,  0,  0,  0, -1,
  -1,  0,  0,  0,  0,  0,  0, -1,
  -1,  0,  0,  0,  0,  0,  0, -1,
  -1,  0,  0,  0,  0,  0,  0, -1,
   1,  2,  2,  2,  2,  2,  2,  1,
   0,  0,  0,  1,  1,  0,  0,  0
};

static const int PST_QUEEN[64] = {
  -2, -1, -1,  0,  0, -1, -1, -2,
  -1,  0,  0,  0,  0,  0,  0, -1,
  -1,  0,  1,  1,  1,  1,  0, -1,
   0,  0,  1,  1,  1,  1,  0,  0,
  -1,  0,  1,  1,  1,  1,  0, -1,
  -1,  0,  1,  1,  1,  1,  0, -1,
  -1,  0,  0,  0,  0,  0,  0, -1,
  -2, -1, -1,  0,  0, -1, -1, -2
};

static const int PST_KING[64] = {
  -3, -4, -4, -5, -5, -4, -4, -3,
  -3, -4, -4, -5, -5, -4, -4, -3,
  -3, -4, -4, -5, -5, -4, -4, -3,
  -3, -4, -4, -5, -5, -4, -4, -3,
  -2, -3, -3, -4, -4, -3, -3, -2,
  -1, -2, -2, -2, -2, -2, -2, -1,
   2,  2,  0,  0,  0,  0,  2,  2,
   2,  3,  1,  0,  0,  1,  3,  2
};

// Sum a PST over all pieces in a bitboard
static inline int pstSum(const int pst[64], Bitboard pieces, bool isWhite) {
  int s = 0;
  Bitboard bb = pieces;
  while (bb) {
    Bitboard one = poplsb(bb);
    int sq = lsb(one);
    int idx = isWhite ? sq : mirror64(sq);
    s += pst[idx];
  }
  return s;
}

// Popcount helper for clarity
static inline int popcount64(Bitboard bb){
  return __builtin_popcountll(bb);
}

// Material values used in the evaluation
static const int VAL_PAWN   = 100;
static const int VAL_KNIGHT = 320;
static const int VAL_BISHOP = 330;
static const int VAL_ROOK   = 500;
static const int VAL_QUEEN  = 900;

int eval(const Board& b){
  int score = 0;

  // 1) Material balance
  score += popcount64(b.pcs[WP]) * VAL_PAWN;
  score += popcount64(b.pcs[WN]) * VAL_KNIGHT;
  score += popcount64(b.pcs[WB]) * VAL_BISHOP;
  score += popcount64(b.pcs[WR]) * VAL_ROOK;
  score += popcount64(b.pcs[WQ]) * VAL_QUEEN;

  score -= popcount64(b.pcs[BP]) * VAL_PAWN;
  score -= popcount64(b.pcs[BN]) * VAL_KNIGHT;
  score -= popcount64(b.pcs[BB]) * VAL_BISHOP;
  score -= popcount64(b.pcs[BR]) * VAL_ROOK;
  score -= popcount64(b.pcs[BQ]) * VAL_QUEEN;

  // 2) Piece square tables
  int pstWhite =
      pstSum(PST_PAWN,   b.pcs[WP], true) +
      pstSum(PST_KNIGHT, b.pcs[WN], true) +
      pstSum(PST_BISHOP, b.pcs[WB], true) +
      pstSum(PST_ROOK,   b.pcs[WR], true) +
      pstSum(PST_QUEEN,  b.pcs[WQ], true) +
      pstSum(PST_KING,   b.pcs[WK], true);

  int pstBlack =
      pstSum(PST_PAWN,   b.pcs[BP], false) +
      pstSum(PST_KNIGHT, b.pcs[BN], false) +
      pstSum(PST_BISHOP, b.pcs[BB], false) +
      pstSum(PST_ROOK,   b.pcs[BR], false) +
      pstSum(PST_QUEEN,  b.pcs[BQ], false) +
      pstSum(PST_KING,   b.pcs[BK], false);

  score += pstWhite;
  score -= pstBlack;

  // Return from side to move perspective
  return (b.stm == WHITE) ? score : -score;
}