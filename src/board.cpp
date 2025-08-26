#include "board.h"
#include "bitboard.h"
#include <cstring>

// File: board.cpp
// Purpose: Hold board state and update it with makeMove and unmakeMove.
// Notes:
//  - Bitboards in pcs[] hold piece locations.
//  - occ[WHITE], occ[BLACK], and occAll are derived. Call set_occ() after edits.
//  - ply counts half moves made from the start of the session.
//  - epSquare is the en passant target square index, or -1 if none.
//  - castleRights bits: 0 Wk, 1 Wq, 2 Bk, 3 Bq.

void Board::clear(){
  std::memset(pcs, 0, sizeof(pcs));
  occ[WHITE] = occ[BLACK] = occAll = 0ULL;
  stm = WHITE;
  ply = 0;
  epSquare = -1;
  castleRights = 0;
}

bool Board::loadFEN(const std::string& fen){
  clear();
  if(fen == "startpos"){
    // White pieces
    pcs[WR] = (1ULL<<0) | (1ULL<<7);
    pcs[WN] = (1ULL<<1) | (1ULL<<6);
    pcs[WB] = (1ULL<<2) | (1ULL<<5);
    pcs[WQ] = (1ULL<<3);
    pcs[WK] = (1ULL<<4);
    pcs[WP] = 0x000000000000FF00ULL;
    // Black pieces
    pcs[BR] = (1ULL<<56) | (1ULL<<63);
    pcs[BN] = (1ULL<<57) | (1ULL<<62);
    pcs[BB] = (1ULL<<58) | (1ULL<<61);
    pcs[BQ] = (1ULL<<59);
    pcs[BK] = (1ULL<<60);
    pcs[BP] = 0x00FF000000000000ULL;

    set_occ();
    stm = WHITE;
    epSquare = -1;
    castleRights = 0xF; // all four rights enabled at start
    return true;
  }
  return false;
}

void Board::set_occ(){
  occ[WHITE] = pcs[WP] | pcs[WN] | pcs[WB] | pcs[WR] | pcs[WQ] | pcs[WK];
  occ[BLACK] = pcs[BP] | pcs[BN] | pcs[BB] | pcs[BR] | pcs[BQ] | pcs[BK];
  occAll = occ[WHITE] | occ[BLACK];
}

int Board::piece_at(int sq) const{
  Bitboard mask = 1ULL << sq;
  for(int p = WP; p <= BK; ++p){
    if(pcs[p] & mask) return p;
  }
  return NO_PIECE;
}

std::string Board::toFEN() const{
  // Not implemented yet.
  return "startpos-stub";
}

void Board::makeMove(const Move m)
{
  // Save reversible state
  hist[ply].last = m;
  hist[ply].prevEpSquare = epSquare;
  hist[ply].prevCastleRights = castleRights;

  // Identify moving and captured pieces
  int movingPiece = piece_at(m.from);
  int capturedPiece = piece_at(m.to);

  // En passant capture sets captured piece behind the to square
  if(m.flags & MF_ENPASSANT){
    capturedPiece = (stm == WHITE) ? BP : WP;
  }
  hist[ply].captured = capturedPiece;

  // Remove captured piece from the board
  if(capturedPiece != NO_PIECE){
    if(m.flags & MF_ENPASSANT){
      int capSq = (stm == WHITE) ? (m.to - 8) : (m.to + 8);
      pcs[capturedPiece] &= ~(1ULL << capSq);
    } else {
      pcs[capturedPiece] &= ~(1ULL << m.to);
    }
  }

  // Clear en passant, may set again on double pawn push
  epSquare = -1;

  // Update castling rights on king and rook moves
  if(movingPiece == WK){ castleRights &= ~(1u << 0); castleRights &= ~(1u << 1); }
  if(movingPiece == BK){ castleRights &= ~(1u << 2); castleRights &= ~(1u << 3); }
  if(movingPiece == WR){ if(m.from == 0) castleRights &= ~(1u << 1); if(m.from == 7) castleRights &= ~(1u << 0); }
  if(movingPiece == BR){ if(m.from == 56) castleRights &= ~(1u << 3); if(m.from == 63) castleRights &= ~(1u << 2); }
  // Update rights if a rook was captured on the original square
  if(capturedPiece == WR){ if(m.to == 0) castleRights &= ~(1u << 1); if(m.to == 7) castleRights &= ~(1u << 0); }
  if(capturedPiece == BR){ if(m.to == 56) castleRights &= ~(1u << 3); if(m.to == 63) castleRights &= ~(1u << 2); }

  // Execute the move
  if(m.promo != 0){
    // Promotion replaces the pawn with the new piece
    pcs[movingPiece] &= ~(1ULL << m.from);
    pcs[m.promo]     |=  (1ULL << m.to);
  } else if(m.flags & MF_CASTLE){
    // Move king first
    pcs[movingPiece] &= ~(1ULL << m.from);
    pcs[movingPiece] |=  (1ULL << m.to);
    // Move rook based on side and destination
    if(stm == WHITE){
      if(m.to == 6){ // e1g1
        pcs[WR] &= ~(1ULL << 7);
        pcs[WR] |=  (1ULL << 5);
      } else if(m.to == 2){ // e1c1
        pcs[WR] &= ~(1ULL << 0);
        pcs[WR] |=  (1ULL << 3);
      }
    } else { // Black
      if(m.to == 62){ // e8g8
        pcs[BR] &= ~(1ULL << 63);
        pcs[BR] |=  (1ULL << 61);
      } else if(m.to == 58){ // e8c8
        pcs[BR] &= ~(1ULL << 56);
        pcs[BR] |=  (1ULL << 59);
      }
    }
  } else {
    // Normal move
    pcs[movingPiece] &= ~(1ULL << m.from);
    pcs[movingPiece] |=  (1ULL << m.to);

    // Set en passant target on a double pawn push
    if(movingPiece == WP && (m.to - m.from) == 16){ epSquare = m.from + 8; }
    if(movingPiece == BP && (m.from - m.to) == 16){ epSquare = m.from - 8; }
  }

  set_occ();
  stm = (stm == WHITE) ? BLACK : WHITE;
  ++ply;
}

void Board::unmakeMove(const Move m)
{
  // Step back ply and side
  --ply;
  stm = (stm == WHITE) ? BLACK : WHITE;

  // Restore EP and castling rights
  epSquare = hist[ply].prevEpSquare;
  castleRights = hist[ply].prevCastleRights;

  // Promotion rollback
  if(m.promo != 0){
    pcs[m.promo] &= ~(1ULL << m.to);
    int pawnPiece = (stm == WHITE) ? BP : WP; // mover was opposite of current stm
    pcs[pawnPiece] |= (1ULL << m.from);

    int cap = hist[ply].captured;
    if(cap != NO_PIECE && cap != -1){
      // EP cannot coincide with promotion, restore on to
      pcs[cap] |= (1ULL << m.to);
    }
    set_occ();
    return;
  }

  // Move the piece back from to to from
  int pieceOnTo = piece_at(m.to);
  pcs[pieceOnTo] &= ~(1ULL << m.to);
  pcs[pieceOnTo] |=  (1ULL << m.from);

  // Rook rollback for castling
  if(m.flags & MF_CASTLE){
    if(stm == WHITE){ // mover was white
      if(m.to == 6){ // e1g1
        pcs[WR] &= ~(1ULL << 5);
        pcs[WR] |=  (1ULL << 7);
      } else if(m.to == 2){ // e1c1
        pcs[WR] &= ~(1ULL << 3);
        pcs[WR] |=  (1ULL << 0);
      }
    } else { // mover was black
      if(m.to == 62){ // e8g8
        pcs[BR] &= ~(1ULL << 61);
        pcs[BR] |=  (1ULL << 63);
      } else if(m.to == 58){ // e8c8
        pcs[BR] &= ~(1ULL << 59);
        pcs[BR] |=  (1ULL << 56);
      }
    }
  }

  // Restore captured piece
  int cap = hist[ply].captured;
  if(cap != NO_PIECE && cap != -1){
    if(m.flags & MF_ENPASSANT){
      int capSq = (stm == WHITE) ? (m.to - 8) : (m.to + 8); // mover was opposite
      pcs[cap] |= (1ULL << capSq);
    } else {
      pcs[cap] |= (1ULL << m.to);
    }
  }

  set_occ();
}

bool Board::inCheck(Color c) const {
  int kingSquare;
  if(c == WHITE){
    if(pcs[WK] == 0) return false; // no king found, treat as not in check
    kingSquare = lsb(pcs[WK]);
  } else {
    if(pcs[BK] == 0) return false;
    kingSquare = lsb(pcs[BK]);
  }
  Bitboard kingMask = 1ULL << kingSquare;

  // Pawn attacks on the king
  if(c == WHITE){
    Bitboard blackAtt = ((pcs[BP] & ~FILE_H) >> 7) | ((pcs[BP] & ~FILE_A) >> 9);
    if(blackAtt & kingMask) return true;
  } else {
    Bitboard whiteAtt = ((pcs[WP] & ~FILE_A) << 7) | ((pcs[WP] & ~FILE_H) << 9);
    if(whiteAtt & kingMask) return true;
  }

  // Knight attacks on the king
  {
    const int dRank[8] = { 2, 2,-2,-2, 1, 1,-1,-1 };
    const int dFile[8] = { 1,-1, 1,-1, 2,-2, 2,-2 };
    int rank = kingSquare / 8, file = kingSquare % 8;
    for(int i = 0; i < 8; ++i){
      int rr = rank + dRank[i], ff = file + dFile[i];
      if(rr >= 0 && rr < 8 && ff >= 0 && ff < 8){
        int sq = rr*8 + ff;
        Bitboard m = 1ULL << sq;
        if(c == WHITE){ if(pcs[BN] & m) return true; }
        else           { if(pcs[WN] & m) return true; }
      }
    }
  }

  // King adjacency
  {
    int rank = kingSquare / 8, file = kingSquare % 8;
    for(int dr = -1; dr <= 1; ++dr){
      for(int df = -1; df <= 1; ++df){
        if(dr == 0 && df == 0) continue;
        int rr = rank + dr, ff = file + df;
        if(rr >= 0 && rr < 8 && ff >= 0 && ff < 8){
          int sq = rr*8 + ff;
          Bitboard m = 1ULL << sq;
          if(c == WHITE){ if(pcs[BK] & m) return true; }
          else           { if(pcs[WK] & m) return true; }
        }
      }
    }
  }

  // Bishop or queen on diagonals
  {
    const int dRank[4] = { 1, 1,-1,-1 };
    const int dFile[4] = { 1,-1, 1,-1 };
    int rank = kingSquare / 8, file = kingSquare % 8;
    for(int d = 0; d < 4; ++d){
      int rr = rank + dRank[d], ff = file + dFile[d];
      while(rr >= 0 && rr < 8 && ff >= 0 && ff < 8){
        int sq = rr*8 + ff;
        Bitboard m = 1ULL << sq;
        if(occAll & m){
          if(c == WHITE){ if((pcs[BB] | pcs[BQ]) & m) return true; }
          else           { if((pcs[WB] | pcs[WQ]) & m) return true; }
          break; // blocked
        }
        rr += dRank[d]; ff += dFile[d];
      }
    }
  }

  // Rook or queen on ranks and files
  {
    const int dRank[4] = { 1,-1, 0, 0 };
    const int dFile[4] = { 0, 0, 1,-1 };
    int rank = kingSquare / 8, file = kingSquare % 8;
    for(int d = 0; d < 4; ++d){
      int rr = rank + dRank[d], ff = file + dFile[d];
      while(rr >= 0 && rr < 8 && ff >= 0 && ff < 8){
        int sq = rr*8 + ff;
        Bitboard m = 1ULL << sq;
        if(occAll & m){
          if(c == WHITE){ if((pcs[BR] | pcs[BQ]) & m) return true; }
          else           { if((pcs[WR] | pcs[WQ]) & m) return true; }
          break; // blocked
        }
        rr += dRank[d]; ff += dFile[d];
      }
    }
  }

  return false;
}