#pragma once
#include <vector>
#include "board.h"
#include "move.h"

// File: movegen.h
// Purpose: Generate legal or pseudo-legal moves and provide perft utilities.

// Generate all pseudo-legal moves for the given position into 'out'.
// Caller must later filter for legality if needed.
void generateMoves(const Board& b, std::vector<Move>& out);

// Perft: count leaf nodes by playing all moves to given depth.
unsigned long long perft(Board& b, int depth);

// Perft divide: print breakdown of nodes per root move.
void perftDivide(Board& b, int depth);