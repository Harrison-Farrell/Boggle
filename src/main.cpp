// system includes
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// local includes
#include "BoggleBoard/BoggleBoard.h"
#include "BoggleSolver/BoggleSolver.h"
#include "Dictionary/CompactTrie.h"
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

  std::vector<std::string_view> found_words;
  const auto start_time = steady_clock::now();
  solver.SolveInto(board.GetFlattenedBoard(), found_words);
  const auto elapsed =
      duration<double, std::micro>(steady_clock::now() - start_time);

  std::cout << "\nFound " << found_words.size() << " words in "
            << elapsed.count() << " us:\n";
  for (const auto &word : found_words) {
    std::cout << word << "\n";
  }
}

/// @brief Shuffles and solves boards spread over several threads
/// @note Each thread has its own board and solver. They share the trie, which
/// is read-only. The found words are ignored.
/// @param trie The trie shared by every solver
/// @param number_of_boards Total boards to shuffle and solve
/// @param thread_count Number of threads to use
/// @return Elapsed time in seconds
double TimeBoards(const std::shared_ptr<const CompactTrie> &trie,
                  uint32_t number_of_boards, uint32_t thread_count) {
  using namespace std::chrono;

  // Threads take boards in chunks as they finish, rather than an equal share
  // up front, so fast cores (or ones the OS is not busy with) do more of the
  // work. This matters on CPUs that mix performance and efficiency cores.
  constexpr uint32_t chunk_size = 1000;
  std::atomic<uint32_t> next_board{0};

  const auto start_time = steady_clock::now();
  {
    std::vector<std::jthread> workers;
    workers.reserve(thread_count);
    for (uint32_t thread = 0; thread < thread_count; ++thread) {
      workers.emplace_back([&trie, &next_board, number_of_boards] {
        BoggleBoard board;
        BoggleSolver solver(trie);
        std::vector<std::string_view> found_words;
        while (true) {
          const uint32_t first = next_board.fetch_add(chunk_size);
          if (first >= number_of_boards) {
            break;
          }
          const uint32_t last = std::min(first + chunk_size, number_of_boards);
          for (uint32_t i = first; i < last; ++i) {
            board.Shuffle();
            solver.SolveInto(board.GetFlattenedBoard(), found_words);
          }
        }
      });
    }
  } // std::jthread joins on destruction
  return duration<double>(steady_clock::now() - start_time).count();
}

/// @brief Attempts to solve one million boards, first on one thread and then
/// on every hardware thread. Prints the time taken.
/// @note The board is shuffled between each solve. The found words are ignored.
/// @param trie The trie built from the dictionary
void SolveOneMillionBoards(const std::shared_ptr<const CompactTrie> &trie) {
  constexpr uint32_t number_of_boards = 1000000;
  const uint32_t thread_count =
      std::max(1U, std::thread::hardware_concurrency());

  std::cout << "\n----------------------------------------------------------\n"
            << "Timing how long it takes to solve 1 million random boards:\n"
            << "NOTE: This time includes the shuffling of the board\n"
            << "It shouldn't take too long... ... ...\n\n";

  const auto report = [](const char *label, double seconds) {
    std::cout << label << ": solved " << number_of_boards << " boards in "
              << seconds << " s, average " << seconds / number_of_boards * 1e6
              << " us per board to shuffle & solve\n";
  };

  const double single_seconds = TimeBoards(trie, number_of_boards, 1);
  report("1 thread  ", single_seconds);

  const double multi_seconds = TimeBoards(trie, number_of_boards, thread_count);
  const std::string label = std::to_string(thread_count) + " threads";
  report(label.c_str(), multi_seconds);

  std::cout << "Speed-up from threads: " << single_seconds / multi_seconds
            << "x\n\n";
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

  const auto trie = BoggleSolver::BuildTrie(dictionary);
  BoggleBoard board;
  BoggleSolver solver(trie);

  SolveBoard(solver, board);

  SolveOneMillionBoards(trie);

  return 0;
}
