#include "board.h"
#include "movegen.h"
#include "search.h"
#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>

// File: uci.cpp
// Purpose: Command-line front-end for blitz engine.
// Supports commands:
//   blitz perft N        -> run perft to depth N
//   blitz divide N       -> per-move perft breakdown
//   blitz search depth N -> search to depth N, print bestmove and PV
//   blitz play           -> interactive play mode

// Convert internal Move to UCI string (e.g. "e2e4", "e7e8q")
static std::string move_to_uci(const Move& m) {
  std::string s;
  s.push_back(char('a' + (m.from % 8)));
  s.push_back(char('1' + (m.from / 8)));
  s.push_back(char('a' + (m.to   % 8)));
  s.push_back(char('1' + (m.to   / 8)));
  if (m.promo) {
    char pc = 'q';
    if (m.promo == WR || m.promo == BR) pc = 'r';
    else if (m.promo == WB || m.promo == BB) pc = 'b';
    else if (m.promo == WN || m.promo == BN) pc = 'n';
    s.push_back(pc);
  }
  return s;
}

// Print usage help
static void print_usage() {
  std::cout << "usage: blitz perft N\n";
  std::cout << "       blitz divide N\n";
  std::cout << "       blitz search depth N\n";
  std::cout << "       blitz play\n";
}

// Compare two moves by core fields
static bool same_move(const Move& a, const Move& b) {
  return a.from == b.from && a.to == b.to && a.promo == b.promo;
}

// Parse a UCI string like "e2e4" or "e7e8q" into a Move with promo resolved for side to move
static bool parse_uci_move(const Board& b, const std::string& s, Move& out){
  if(s.size() < 4) return false;
  int ffile = s[0] - 'a';
  int frank = s[1] - '1';
  int tfile = s[2] - 'a';
  int trank = s[3] - '1';
  if(ffile < 0 || ffile > 7 || tfile < 0 || tfile > 7 || frank < 0 || frank > 7 || trank < 0 || trank > 7) return false;
  int from = frank * 8 + ffile;
  int to   = trank * 8 + tfile;
  int promo = 0;
  if(s.size() >= 5){
    char pc = std::tolower(s[4]);
    if(b.stm == WHITE){
      if(pc == 'q') promo = WQ; else if(pc == 'r') promo = WR; else if(pc == 'b') promo = WB; else if(pc == 'n') promo = WN; else return false;
    } else {
      if(pc == 'q') promo = BQ; else if(pc == 'r') promo = BR; else if(pc == 'b') promo = BB; else if(pc == 'n') promo = BN; else return false;
    }
  }
  out = Move{(uint16_t)from,(uint16_t)to,(uint8_t)promo,(uint8_t)0};
  return true;
}

// Check if a move is legal in the given position by generating moves and matching
static bool legal_in_pos(const Board& b, const Move& m) {
  std::vector<Move> mv;
  generateMoves(b, mv);
  for (const auto& x : mv) if (same_move(x, m)) return true;
  return false;
}

// Print a tiny prompt showing side to move and depth
static void print_prompt(const Board& b, int depth){
  std::cout << ((b.stm == WHITE) ? "white" : "black") << " to move | depth " << depth << "\n> " << std::flush;
}

int main(int argc, char* argv[]) {
  Board b;
  b.loadFEN("startpos");

  // perft N
  if (argc == 3 && std::string(argv[1]) == "perft") {
    int depth = std::stoi(argv[2]);
    unsigned long long nodes = perft(b, depth);
    std::cout << "Perft(" << depth << ") = " << nodes << "\n";
    return 0;
  }

  // divide N
  if (argc == 3 && std::string(argv[1]) == "divide") {
    int depth = std::stoi(argv[2]);
    perftDivide(b, depth);
    return 0;
  }

  // search depth N
  if (argc == 4 && std::string(argv[1]) == "search" && std::string(argv[2]) == "depth") {
    int depth = std::stoi(argv[3]);
    SearchResult res = search_root(b, depth);

    // Info line for UCI
    std::cout << "info score cp " << res.score << " nodes " << res.nodes << "\n";

    // Best move with principal variation
    std::cout << "bestmove " << move_to_uci(res.best);
    if (!res.pv.empty()) {
      std::cout << " pv";
      for (const auto& m : res.pv) {
        std::cout << " " << move_to_uci(m);
      }
    }
    std::cout << "\n";
    return 0;
  }

  // play (interactive)
  if (argc == 2 && std::string(argv[1]) == "play") {
    int depth = 4; // default search depth
    std::vector<Move> played; // moves we applied, to support undo

    std::cout << "blitz interactive. Commands: move in UCI (e2e4), go, depth N, undo, reset, help, quit\n";
    print_prompt(b, depth);
    std::string line;
    while (std::getline(std::cin, line)) {
      // trim
      while(!line.empty() && (line.back()=='\r' || line.back()=='\n' || line.back()==' ')) line.pop_back();
      if(line.empty()){ print_prompt(b, depth); continue; }

      if (line == "quit" || line == "exit") break;
      if (line == "help") {
        std::cout << "Commands: UCI move like e2e4, go, depth N, undo, reset, quit\n";
        print_prompt(b, depth);
        continue;
      }
      if (line.rfind("depth ", 0) == 0) {
        int d = std::max(1, std::atoi(line.c_str()+6));
        depth = d;
        std::cout << "depth set to " << depth << "\n";
        print_prompt(b, depth);
        continue;
      }
      if (line == "reset") {
        b.loadFEN("startpos");
        played.clear();
        std::cout << "reset to startpos\n";
        print_prompt(b, depth);
        continue;
      }
      if (line == "undo") {
        if (!played.empty()) {
          Move last = played.back();
          played.pop_back();
          b.unmakeMove(last);
          std::cout << "undone\n";
        } else {
          std::cout << "nothing to undo\n";
        }
        print_prompt(b, depth);
        continue;
      }
      if (line == "go") {
        SearchResult res = search_root(b, depth);
        std::cout << "info score cp " << res.score << " nodes " << res.nodes << "\n";
        std::cout << "bestmove " << move_to_uci(res.best);
        if (!res.pv.empty()) { std::cout << " pv"; for (const auto& m : res.pv) std::cout << " " << move_to_uci(m); }
        std::cout << "\n";
        if (res.best.from || res.best.to) { b.makeMove(res.best); played.push_back(res.best); }
        print_prompt(b, depth);
        continue;
      }

      // Try to parse a user move in UCI
      Move m{};
      if (parse_uci_move(b, line, m)) {
        if (legal_in_pos(b, m)) {
          b.makeMove(m);
          played.push_back(m);
          print_prompt(b, depth);
        } else {
          std::cout << "illegal move\n";
          print_prompt(b, depth);
        }
        continue;
      }

      std::cout << "unknown command\n";
      print_prompt(b, depth);
    }
    return 0;
  }

  print_usage();
  return 0;
}