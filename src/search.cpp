#include <algorithm>
#include <vector>
#include "search.h"
#include "eval.h"
#include "bitboard.h"

// File: search.cpp
// Purpose: Game tree search helpers and root search. Uses negamax with alpha-beta.
// Notes:
//  - Scores are from the side to move perspective.
//  - Captures are ordered by MVV-LVA to improve pruning.
//  - Principal variation is built at the root only.

// Value used for MVV scoring
static inline int pieceValueForMVV(int p){
  switch(p){
    case WP: case BP: return 100;
    case WN: case BN: return 320;
    case WB: case BB: return 330;
    case WR: case BR: return 500;
    case WQ: case BQ: return 900;
    default: return 0;
  }
}

// True if move m captures an opponent piece or is en passant
static inline bool isCapture(const Board& b, const Move& m){
  Color us = b.stm;
  Color them = (us == WHITE) ? BLACK : WHITE;
  Bitboard toMask = 1ULL << m.to;
  return (b.occ[them] & toMask) != 0 || (m.flags & MF_ENPASSANT);
}

// Most Valuable Victim, Least Valuable Attacker score for move ordering
static inline int mvvLva(const Board& b, const Move& m){
  if(!isCapture(b, m)) return 0;
  int victim = b.piece_at(m.to);
  if(m.flags & MF_ENPASSANT) victim = (b.stm == WHITE) ? BP : WP;
  int attacker = b.piece_at(m.from);
  return pieceValueForMVV(victim) * 10 - pieceValueForMVV(attacker);
}

// Negamax with alpha-beta pruning
// Returns a score from the side to move perspective
static int negamax(Board& b, int depth, int alpha, int beta, unsigned long long& nodeCount){
  if(depth == 0){
    ++nodeCount;
    return eval(b);
  }

  std::vector<Move> legalMoves;
  generateMoves(b, legalMoves);

  // No legal moves, checkmate or stalemate
  if(legalMoves.empty()){
    ++nodeCount;
    if(b.inCheck(b.stm)){
      return -100000 + b.ply; // checkmate, prefer faster mates for the winner
    }
    return 0; // stalemate
  }

  // Order moves, captures first by MVV-LVA
  std::stable_sort(legalMoves.begin(), legalMoves.end(), [&](const Move& a, const Move& z){
    return mvvLva(b, a) > mvvLva(b, z);
  });

  int bestScore = -10000000;
  for(const Move& mv : legalMoves){
    b.makeMove(mv);
    int score = -negamax(b, depth - 1, -beta, -alpha, nodeCount);
    b.unmakeMove(mv);

    if(score > bestScore) bestScore = score;
    if(score > alpha) alpha = score;
    if(alpha >= beta) break; // beta cutoff
  }
  return bestScore;
}

// Choose the single best move in the given position for a given depth
// Used to build the root principal variation without threading PV through all nodes
static Move chooseBestMove(Board& b, int depth, unsigned long long& nodeCount){
  std::vector<Move> legalMoves;
  generateMoves(b, legalMoves);
  if(legalMoves.empty()) return Move{};

  std::stable_sort(legalMoves.begin(), legalMoves.end(), [&](const Move& a, const Move& z){
    return mvvLva(b, a) > mvvLva(b, z);
  });

  int alpha = -10000000;
  int beta  =  10000000;
  int bestScore = -10000000;
  Move bestMove{};

  for(const Move& mv : legalMoves){
    b.makeMove(mv);
    int score = -negamax(b, depth - 1, -beta, -alpha, nodeCount);
    b.unmakeMove(mv);

    if(score > bestScore){
      bestScore = score;
      bestMove = mv;
    }
    if(score > alpha) alpha = score;
  }
  return bestMove;
}

// Root search, builds a one-line principal variation by walking best replies
SearchResult search_root(Board& b, int depth){
  SearchResult out{};
  out.nodes = 0ULL;

  std::vector<Move> legalMoves;
  generateMoves(b, legalMoves);

  std::stable_sort(legalMoves.begin(), legalMoves.end(), [&](const Move& a, const Move& z){
    return mvvLva(b, a) > mvvLva(b, z);
  });

  int alpha = -10000000;
  int beta  =  10000000;
  int bestScore = -10000000;
  Move bestMove{};

  for(const Move& mv : legalMoves){
    b.makeMove(mv);
    unsigned long long childNodes = 0ULL;
    int score = -negamax(b, depth - 1, -beta, -alpha, childNodes);
    b.unmakeMove(mv);

    out.nodes += childNodes;
    if(score > bestScore){
      bestScore = score;
      bestMove = mv;
    }
    if(score > alpha) alpha = score;
  }

  out.best = bestMove;
  out.score = bestScore;

  // Build the principal variation at the root by walking best responses
  out.pv.clear();
  if(bestMove.from || bestMove.to){
    out.pv.push_back(bestMove);
    Board walk = b;
    walk.makeMove(bestMove);
    int d = depth - 1;
    while(d > 0){
      unsigned long long extraNodes = 0ULL;
      Move next = chooseBestMove(walk, d, extraNodes);
      out.nodes += extraNodes;
      if(!(next.from || next.to)) break; // no more legal moves
      out.pv.push_back(next);
      walk.makeMove(next);
      --d;
    }
  }

  return out;
}