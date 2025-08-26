#pragma once
#include "board.h"
#include "movegen.h"
struct SearchResult { Move best; int score; unsigned long long nodes; std::vector<Move> pv;}; //pv = principal variation
SearchResult search_root(Board& b, int depth);