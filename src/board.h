#pragma once
#include <string>
#include "bitboard.h"
#include "move.h"

// File: board.h
// Purpose: Define board state, piece/side enums, and board manipulation API.
// Notes:
//  - Bitboards in pcs[] hold piece positions for each type.
//  - occ[WHITE], occ[BLACK], and occAll track occupancy.
//  - State hist[] stores reversible info for unmakeMove.
//  - ply counts half-moves from the root.

// Side to move
enum Color { WHITE = 0, BLACK = 1 };

// Piece indices into pcs[]
enum Piece {
  WP, WN, WB, WR, WQ, WK,
  BP, BN, BB, BR, BQ, BK,
  NO_PIECE
};

// Move flag bits
enum MoveFlags : uint8_t {
  MF_NONE      = 0,
  MF_CAPTURE   = 1 << 0,
  MF_ENPASSANT = 1 << 1,
  MF_CASTLE    = 1 << 2,
  MF_DOUBLE    = 1 << 3
};

// Reversible state stored per ply
struct State {
  Move last{};              // last move played
  int captured{-1};         // piece captured, or -1 if none
  int prevEpSquare{-1};     // en passant target before move, -1 if none
  uint8_t prevCastleRights{0}; // castling rights before move
};

// Board holds all game state
class Board {
public:
  Bitboard pcs[12]{};   // piece bitboards, indexed by Piece
  Bitboard occ[2]{};    // occupancy for each side
  Bitboard occAll{};    // all occupied squares
  int epSquare{-1};     // en passant target square, -1 if none
  uint8_t castleRights{0}; // bit0: WK, bit1: WQ, bit2: BK, bit3: BQ
  Color stm{WHITE};     // side to move
  State hist[512]{};    // move history for undo
  int ply{0};           // half-move count from root

  void clear();
  bool loadFEN(const std::string& fen); // only supports "startpos" for now
  std::string toFEN() const;
  int piece_at(int sq) const;           // return Piece enum or NO_PIECE
  void set_occ();
  void makeMove(const Move m);          // apply move, supports captures, promo, ep, castle
  void unmakeMove(const Move m);        // undo move, restores captures/promo/ep/castle
  bool inCheck(Color c) const;          // true if side c's king is attacked
};