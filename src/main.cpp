// system includes
#include <chrono>
#include <iostream>
#include <string>

// local includes
#include "BoggleBoard/BoggleBoard.h"
#include "BoggleSolver/BoggleSolver.h"
#include "Dictionary/Dictionary.h"

namespace {
/// @brief Print usage information
void PrintHelp() {
  std::cout << "\n--- Boggle Solver Usage ---\n"
            << "Expected:\t"
            << "  <path to dictionary file>" << "\n"
            << "Example:\t"
            << "  SolveBoggle  words.txt\n"
            << "---------------------------\n";
}

/// @brief Shuffles & prints the board. Attemps to find words. Timing the
/// attempt. Printing the found words to standard out.
/// @param solver Reference to the BoggleSolver
/// @param board Reference to the BoggleBoard
void SolveBoard(BoggleSolver &solver, BoggleBoard &board) {
  using namespace std::chrono;

  board.Shuffle();
  board.PrintBoard();

  auto start_time = high_resolution_clock::now().time_since_epoch().count();
  auto found_words = solver.Solve(board.GetFlattenedBoard());
  auto end_time = high_resolution_clock::now().time_since_epoch().count();
  auto duration = end_time - start_time;

  std::cout << "\nFound " << found_words.size() << " words in "
            << static_cast<double>(duration) / 1e3 << " us:\n";
  for (const auto &word : found_words) {
    std::cout << word << "\n";
  }
}

/// @brief Attempts to solve one million boards. Prints the time taken.
/// @note The board is shuffled between each solve. The found words are ignored.
/// @param solver Reference to the BoggleSolver
/// @param board Reference to teh BoggleBoard
void SolveOneMillionBoards(BoggleSolver &solver, BoggleBoard &board) {
  using namespace std::chrono;
  constexpr const int number_of_boards = 1000000;

  std::cout << "\n----------------------------------------------------------\n"
            << "Timing how long it takes to solve 1 million random boards:\n"
            << "NOTE: This time includes the shuffling of the board\n"
            << "It shouldn't take too long... ... ...\n\n";

  auto start_time = high_resolution_clock::now().time_since_epoch().count();
  for (int i = 0; i < number_of_boards; ++i) {
    board.Shuffle();
    auto found_words = solver.Solve(board.GetFlattenedBoard());
  }
  auto end_time = high_resolution_clock::now().time_since_epoch().count();
  auto duration = end_time - start_time;

  std::cout << "Solved " << number_of_boards << " boards in "
            << static_cast<double>(duration) / 1e9 << " s\n"
            << "Average time to shuffle & solve a board: "
            << static_cast<double>(duration) / number_of_boards / 1e3
            << " us\n\n";
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Error: Invalid number of arguments provided.\n";
    PrintHelp();
    return 1;
  }

  const std::string file_path = argv[1];

  std::cout << "Building Dictionary from: " << file_path << "...\n";
  auto dictionary = ReadWordsFromFile(file_path);

  if (dictionary == nullptr) {
    std::cerr << "Critical Error: Failed to construct the Dictionary from "
                 "the provided file.\n";
    return 1;
  }

  std::cout << "Successfully loaded dictionary into the Dictionary!\n";
  std::cout << "\nSolving the following board:\n";

  BoggleBoard board;
  BoggleSolver solver(dictionary);

  SolveBoard(solver, board);

  SolveOneMillionBoards(solver, board);

  return 0;
}
