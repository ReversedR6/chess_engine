#include <vector>
#include "movegen.h"
#include "bitboard.h"

// File: movegen.cpp
// Purpose: Generate pseudo-legal moves, then filter to legal by king safety.
// Also provides perft utilities.

// Precomputed attack masks for knights and kings
static Bitboard knightAttacks[64];
static Bitboard kingAttacks[64];

static inline bool onBoard(int sq){ return sq >= 0 && sq < 64; }

static void initKnight(){
  for(int sq = 0; sq < 64; ++sq){
    int r = sq / 8, f = sq % 8;
    Bitboard mask = 0;
    const int dR[8] = { 2, 2,-2,-2, 1, 1,-1,-1 };
    const int dF[8] = { 1,-1, 1,-1, 2,-2, 2,-2 };
    for(int k = 0; k < 8; ++k){
      int rr = r + dR[k], ff = f + dF[k];
      if(rr >= 0 && rr < 8 && ff >= 0 && ff < 8){
        mask |= 1ULL << (rr*8 + ff);
      }
    }
    knightAttacks[sq] = mask;
  }
}

static void initKing(){
  for(int sq = 0; sq < 64; ++sq){
    int r = sq / 8, f = sq % 8;
    Bitboard mask = 0;
    for(int dr = -1; dr <= 1; ++dr){
      for(int df = -1; df <= 1; ++df){
        if(dr == 0 && df == 0) continue;
        int rr = r + dr, ff = f + df;
        if(rr >= 0 && rr < 8 && ff >= 0 && ff < 8){
          mask |= 1ULL << (rr*8 + ff);
        }
      }
    }
    kingAttacks[sq] = mask;
  }
}

static bool initDone = false;
static void ensureInit(){ if(!initDone){ initKnight(); initKing(); initDone = true; } }

// True if square sq is attacked by the given side
static inline bool attackedBy(const Board& b, int sq, Color by){
  const Bitboard target = 1ULL << sq;

  // Pawn attacks
  if(by == WHITE){
    Bitboard att = ((b.pcs[WP] & ~FILE_A) << 7) | ((b.pcs[WP] & ~FILE_H) << 9);
    if(att & target) return true;
  } else {
    Bitboard att = ((b.pcs[BP] & ~FILE_H) >> 7) | ((b.pcs[BP] & ~FILE_A) >> 9);
    if(att & target) return true;
  }

  // Knight attacks
  if(by == WHITE){ if(knightAttacks[sq] & b.pcs[WN]) return true; }
  else            { if(knightAttacks[sq] & b.pcs[BN]) return true; }

  // King adjacency
  if(by == WHITE){ if(kingAttacks[sq] & b.pcs[WK]) return true; }
  else            { if(kingAttacks[sq] & b.pcs[BK]) return true; }

  // Sliding attacks: bishops and queens on diagonals
  {
    const int dR[4] = { 1, 1,-1,-1 };
    const int dF[4] = { 1,-1, 1,-1 };
    int r0 = sq / 8, f0 = sq % 8;
    for(int d = 0; d < 4; ++d){
      int r = r0 + dR[d], f = f0 + dF[d];
      while(r >= 0 && r < 8 && f >= 0 && f < 8){
        int s = r*8 + f;
        Bitboard m = 1ULL << s;
        if(b.occAll & m){
          if(by == WHITE){ if((b.pcs[WB] | b.pcs[WQ]) & m) return true; }
          else            { if((b.pcs[BB] | b.pcs[BQ]) & m) return true; }
          break;
        }
        r += dR[d]; f += dF[d];
      }
    }
  }

  // Sliding attacks: rooks and queens on ranks and files
  {
    const int dR[4] = { 1,-1, 0, 0 };
    const int dF[4] = { 0, 0, 1,-1 };
    int r0 = sq / 8, f0 = sq % 8;
    for(int d = 0; d < 4; ++d){
      int r = r0 + dR[d], f = f0 + dF[d];
      while(r >= 0 && r < 8 && f >= 0 && f < 8){
        int s = r*8 + f;
        Bitboard m = 1ULL << s;
        if(b.occAll & m){
          if(by == WHITE){ if((b.pcs[WR] | b.pcs[WQ]) & m) return true; }
          else            { if((b.pcs[BR] | b.pcs[BQ]) & m) return true; }
          break;
        }
        r += dR[d]; f += dF[d];
      }
    }
  }

  return false;
}

// Push all moves encoded by mask from a given origin square
static inline void pushMovesFromMask(std::vector<Move>& out, int from, Bitboard mask){
  Bitboard m = mask;
  while(m){
    Bitboard tobb = poplsb(m);
    int to = lsb(tobb);
    out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
  }
}

// Slide along one direction until a blocker is hit
static inline void slideInDirection(const Board& b, std::vector<Move>& out, int from, int dr, int df){
  int r = from / 8, f = from % 8;
  int rr = r + dr, ff = f + df;
  while(rr >= 0 && rr < 8 && ff >= 0 && ff < 8){
    int to = rr*8 + ff;
    Bitboard toMask = 1ULL << to;
    if(b.occAll & toMask){
      if(b.stm == WHITE){
        if(b.occ[BLACK] & toMask) out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
      } else {
        if(b.occ[WHITE] & toMask) out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
      }
      break; // stop at first blocker
    } else {
      out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
    }
    rr += dr; ff += df;
  }
}

// Generate pseudo-legal moves for all pieces. Adds en passant and castling. Then filter to legal.
void generateMoves(const Board& b, std::vector<Move>& out){
  ensureInit();
  out.clear();
  const Bitboard empty = ~b.occAll;

  if(b.stm == WHITE){
    // Pawn single pushes
    Bitboard singles = (b.pcs[WP] << 8) & empty;
    Bitboard tmp = singles;
    while(tmp){
      Bitboard tobb = poplsb(tmp);
      int to = lsb(tobb);
      int from = to - 8;
      out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
    }
    // Pawn double pushes from rank 2
    Bitboard doubles = ((singles & (RANK_2 << 8)) << 8) & empty;
    tmp = doubles;
    while(tmp){
      Bitboard tobb = poplsb(tmp);
      int to = lsb(tobb);
      int from = to - 16;
      out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
    }
    // Pawn captures
    Bitboard capNW = ((b.pcs[WP] & ~FILE_A) << 7) & b.occ[BLACK];
    Bitboard capNE = ((b.pcs[WP] & ~FILE_H) << 9) & b.occ[BLACK];
    tmp = capNW;
    while(tmp){
      Bitboard tobb = poplsb(tmp);
      int to = lsb(tobb);
      int from = to - 7;
      out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
    }
    tmp = capNE;
    while(tmp){
      Bitboard tobb = poplsb(tmp);
      int to = lsb(tobb);
      int from = to - 9;
      out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
    }

    // En passant
    if(b.epSquare != -1){
      const int epsq = b.epSquare;
      const Bitboard epMask = 1ULL << epsq;
      if((b.pcs[WP] & ~FILE_A) & (epMask >> 7)){
        const int from = epsq - 7;
        out.push_back(Move{(uint16_t)from,(uint16_t)epsq,0,(uint8_t)(MF_ENPASSANT | MF_CAPTURE)});
      }
      if((b.pcs[WP] & ~FILE_H) & (epMask >> 9)){
        const int from = epsq - 9;
        out.push_back(Move{(uint16_t)from,(uint16_t)epsq,0,(uint8_t)(MF_ENPASSANT | MF_CAPTURE)});
      }
    }

    // Knights
    {
      Bitboard kn = b.pcs[WN];
      while(kn){
        Bitboard frombb = poplsb(kn);
        int from = lsb(frombb);
        Bitboard attacks = knightAttacks[from];
        Bitboard quiets = attacks & ~b.occAll;
        Bitboard caps   = attacks & b.occ[BLACK];
        pushMovesFromMask(out, from, quiets);
        pushMovesFromMask(out, from, caps);
      }
    }

    // Bishops
    {
      Bitboard bb = b.pcs[WB];
      while(bb){
        Bitboard frombb = poplsb(bb);
        int from = lsb(frombb);
        slideInDirection(b, out, from,  1,  1);
        slideInDirection(b, out, from,  1, -1);
        slideInDirection(b, out, from, -1,  1);
        slideInDirection(b, out, from, -1, -1);
      }
    }

    // Rooks
    {
      Bitboard rb = b.pcs[WR];
      while(rb){
        Bitboard frombb = poplsb(rb);
        int from = lsb(frombb);
        slideInDirection(b, out, from,  1,  0);
        slideInDirection(b, out, from, -1,  0);
        slideInDirection(b, out, from,  0,  1);
        slideInDirection(b, out, from,  0, -1);
      }
    }

    // Queens
    {
      Bitboard qb = b.pcs[WQ];
      while(qb){
        Bitboard frombb = poplsb(qb);
        int from = lsb(frombb);
        slideInDirection(b, out, from,  1,  1);
        slideInDirection(b, out, from,  1, -1);
        slideInDirection(b, out, from, -1,  1);
        slideInDirection(b, out, from, -1, -1);
        slideInDirection(b, out, from,  1,  0);
        slideInDirection(b, out, from, -1,  0);
        slideInDirection(b, out, from,  0,  1);
        slideInDirection(b, out, from,  0, -1);
      }
    }

    // King
    {
      int from = lsb(b.pcs[WK]);
      Bitboard attacks = kingAttacks[from];
      Bitboard quiets = attacks & ~b.occAll;
      Bitboard caps   = attacks & b.occ[BLACK];
      pushMovesFromMask(out, from, quiets);
      pushMovesFromMask(out, from, caps);
    }

    // Castling
    {
      if(b.pcs[WK] & (1ULL << 4)){
        // King side. Rights bit0. Squares f1 and g1 empty. Rook on h1. Path not attacked.
        if((b.castleRights & (1u << 0)) && ((b.occAll & ((1ULL<<5)|(1ULL<<6))) == 0) && (b.pcs[WR] & (1ULL<<7))){
          if(!attackedBy(b, 4, BLACK) && !attackedBy(b, 5, BLACK) && !attackedBy(b, 6, BLACK)){
            out.push_back(Move{4,6,0,(uint8_t)MF_CASTLE});
          }
        }
        // Queen side. Rights bit1. Squares d1 and c1 empty. Rook on a1. Path not attacked.
        if((b.castleRights & (1u << 1)) && ((b.occAll & ((1ULL<<3)|(1ULL<<2))) == 0) && (b.pcs[WR] & (1ULL<<0))){
          if(!attackedBy(b, 4, BLACK) && !attackedBy(b, 3, BLACK) && !attackedBy(b, 2, BLACK)){
            out.push_back(Move{4,2,0,(uint8_t)MF_CASTLE});
          }
        }
      }
    }

  } else {
    // Black to move
    // Pawn single pushes
    Bitboard singles = (b.pcs[BP] >> 8) & empty;
    Bitboard tmp = singles;
    while(tmp){
      Bitboard tobb = poplsb(tmp);
      int to = lsb(tobb);
      int from = to + 8;
      out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
    }
    // Pawn double pushes from rank 7
    Bitboard doubles = ((singles & (RANK_7 >> 8)) >> 8) & empty;
    tmp = doubles;
    while(tmp){
      Bitboard tobb = poplsb(tmp);
      int to = lsb(tobb);
      int from = to + 16;
      out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
    }
    // Pawn captures
    Bitboard capSE = ((b.pcs[BP] & ~FILE_H) >> 7) & b.occ[WHITE];
    Bitboard capSW = ((b.pcs[BP] & ~FILE_A) >> 9) & b.occ[WHITE];
    tmp = capSE;
    while(tmp){
      Bitboard tobb = poplsb(tmp);
      int to = lsb(tobb);
      int from = to + 7;
      out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
    }
    tmp = capSW;
    while(tmp){
      Bitboard tobb = poplsb(tmp);
      int to = lsb(tobb);
      int from = to + 9;
      out.push_back(Move{(uint16_t)from,(uint16_t)to,0,0});
    }

    // En passant
    if(b.epSquare != -1){
      const int epsq = b.epSquare;
      const Bitboard epMask = 1ULL << epsq;
      if((b.pcs[BP] & ~FILE_H) & (epMask << 7)){
        const int from = epsq + 7;
        out.push_back(Move{(uint16_t)from,(uint16_t)epsq,0,(uint8_t)(MF_ENPASSANT | MF_CAPTURE)});
      }
      if((b.pcs[BP] & ~FILE_A) & (epMask << 9)){
        const int from = epsq + 9;
        out.push_back(Move{(uint16_t)from,(uint16_t)epsq,0,(uint8_t)(MF_ENPASSANT | MF_CAPTURE)});
      }
    }

    // Knights
    {
      Bitboard kn = b.pcs[BN];
      while(kn){
        Bitboard frombb = poplsb(kn);
        int from = lsb(frombb);
        Bitboard attacks = knightAttacks[from];
        Bitboard quiets = attacks & ~b.occAll;
        Bitboard caps   = attacks & b.occ[WHITE];
        pushMovesFromMask(out, from, quiets);
        pushMovesFromMask(out, from, caps);
      }
    }

    // Bishops
    {
      Bitboard bb = b.pcs[BB];
      while(bb){
        Bitboard frombb = poplsb(bb);
        int from = lsb(frombb);
        slideInDirection(b, out, from,  1,  1);
        slideInDirection(b, out, from,  1, -1);
        slideInDirection(b, out, from, -1,  1);
        slideInDirection(b, out, from, -1, -1);
      }
    }

    // Rooks
    {
      Bitboard rb = b.pcs[BR];
      while(rb){
        Bitboard frombb = poplsb(rb);
        int from = lsb(frombb);
        slideInDirection(b, out, from,  1,  0);
        slideInDirection(b, out, from, -1,  0);
        slideInDirection(b, out, from,  0,  1);
        slideInDirection(b, out, from,  0, -1);
      }
    }

    // Queens
    {
      Bitboard qb = b.pcs[BQ];
      while(qb){
        Bitboard frombb = poplsb(qb);
        int from = lsb(frombb);
        slideInDirection(b, out, from,  1,  1);
        slideInDirection(b, out, from,  1, -1);
        slideInDirection(b, out, from, -1,  1);
        slideInDirection(b, out, from, -1, -1);
        slideInDirection(b, out, from,  1,  0);
        slideInDirection(b, out, from, -1,  0);
        slideInDirection(b, out, from,  0,  1);
        slideInDirection(b, out, from,  0, -1);
      }
    }

    // King
    {
      int from = lsb(b.pcs[BK]);
      Bitboard attacks = kingAttacks[from];
      Bitboard quiets = attacks & ~b.occAll;
      Bitboard caps   = attacks & b.occ[WHITE];
      pushMovesFromMask(out, from, quiets);
      pushMovesFromMask(out, from, caps);
    }

    // Castling
    {
      if(b.pcs[BK] & (1ULL << 60)){
        // King side
        if((b.castleRights & (1u << 2)) && ((b.occAll & ((1ULL<<61)|(1ULL<<62))) == 0) && (b.pcs[BR] & (1ULL<<63))){
          if(!attackedBy(b, 60, WHITE) && !attackedBy(b, 61, WHITE) && !attackedBy(b, 62, WHITE)){
            out.push_back(Move{60,62,0,(uint8_t)MF_CASTLE});
          }
        }
        // Queen side
        if((b.castleRights & (1u << 3)) && ((b.occAll & ((1ULL<<59)|(1ULL<<58))) == 0) && (b.pcs[BR] & (1ULL<<56))){
          if(!attackedBy(b, 60, WHITE) && !attackedBy(b, 59, WHITE) && !attackedBy(b, 58, WHITE)){
            out.push_back(Move{60,58,0,(uint8_t)MF_CASTLE});
          }
        }
      }
    }
  }

  // Legality filter. Keep only moves that do not leave our king in check.
  {
    std::vector<Move> legal;
    legal.reserve(out.size());
    Color side = b.stm;
    for(const auto& m : out){
      Board test = b;
      test.makeMove(m);
      if(!test.inCheck(side)) legal.push_back(m);
    }
    out.swap(legal);
  }
}

unsigned long long perft(Board& b, int depth){
  if(depth == 0) return 1ULL;
  std::vector<Move> mv;
  generateMoves(b, mv);
  unsigned long long nodes = 0ULL;
  for(const auto& m : mv){
    b.makeMove(m);
    nodes += perft(b, depth - 1);
    b.unmakeMove(m);
  }
  return nodes;
}

// Convert square index to algebraic like "a1"
static inline const char* sqToStr(int sq){
  static char buf[3];
  buf[0] = char('a' + (sq % 8));
  buf[1] = char('1' + (sq / 8));
  buf[2] = 0;
  return buf;
}

// Print perft breakdown per root move
void perftDivide(Board& b, int depth){
  std::vector<Move> mv;
  generateMoves(b, mv);
  unsigned long long total = 0ULL;
  for(const auto& m : mv){
    b.makeMove(m);
    unsigned long long nodes = perft(b, depth - 1);
    b.unmakeMove(m);
    total += nodes;

    if(m.promo){
      char promoChar = 'q';
      if(m.promo == WQ || m.promo == BQ) promoChar = 'q';
      else if(m.promo == WR || m.promo == BR) promoChar = 'r';
      else if(m.promo == WB || m.promo == BB) promoChar = 'b';
      else if(m.promo == WN || m.promo == BN) promoChar = 'n';
      printf("%s%s%c: %llu\n", sqToStr(m.from), sqToStr(m.to), promoChar, nodes);
    } else {
      printf("%s%s: %llu\n", sqToStr(m.from), sqToStr(m.to), nodes);
    }
  }
  printf("Total: %llu\n", total);
}

// Backward-compatible alias for existing callers
void perft_divide(Board& b, int depth){ perftDivide(b, depth); }