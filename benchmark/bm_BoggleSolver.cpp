// system includes
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// external includes
#include <benchmark/benchmark.h>

// local includes
#include "BoggleBoard/BoggleBoard.h"
#include "BoggleSolver/BoggleSolver.h"
#include "Dictionary/Dictionary.h"

namespace {
// Number of distinct boards each benchmark cycles through. Solving the same
// board repeatedly lets the branch predictor learn that board, which makes
// the solver look ~3x faster than it is on boards it has not seen before.
constexpr size_t BOARD_COUNT = 4096;
constexpr uint32_t BOARD_SEED = 2024;

const std::string SMALL_DICTIONARY = "basic_english_words_2000.txt";
const std::string LARGE_DICTIONARY = "boggle_words_138735.txt";

struct BenchmarkData {
  std::shared_ptr<Dictionary> dictionary;
  std::vector<std::array<char, BOARD_SIZE>> boards;
};

/// @brief Loads a dictionary and a reproducible set of boards, once per file
const BenchmarkData &GetData(const std::string &dictionary_file) {
  static std::mutex mutex;
  static std::map<std::string, BenchmarkData> cache;

  const std::scoped_lock lock(mutex);
  auto [entry, inserted] = cache.try_emplace(dictionary_file);
  if (inserted) {
    entry->second.dictionary = ReadWordsFromFile(dictionary_file);

    BoggleBoard board(BOARD_SEED);
    entry->second.boards.reserve(BOARD_COUNT);
    for (size_t i = 0; i < BOARD_COUNT; ++i) {
      entry->second.boards.push_back(board.GetFlattenedBoard());
      board.Shuffle();
    }
  }
  return entry->second;
}

/// @brief Solves a different board on every iteration
void BM_SolveRandomBoards(benchmark::State &state,
                          const std::string &dictionary_file) {
  const auto &data = GetData(dictionary_file);
  if (!data.dictionary) {
    state.SkipWithError("Could not load " + dictionary_file);
    return;
  }

  BoggleSolver solver(data.dictionary);
  size_t board_index = 0;
  for (auto _ : state) {
    auto words = solver.Solve(data.boards[board_index]);
    benchmark::DoNotOptimize(words);
    board_index = (board_index + 1) % data.boards.size();
  }
  state.SetItemsProcessed(state.iterations());
}

/// @brief Solves the same board on every iteration (the original benchmark,
/// kept so results stay comparable with the optimisation history)
void BM_SolveFixedBoard(benchmark::State &state,
                        const std::string &dictionary_file) {
  const auto &data = GetData(dictionary_file);
  if (!data.dictionary) {
    state.SkipWithError("Could not load " + dictionary_file);
    return;
  }

  BoggleSolver solver(data.dictionary);
  for (auto _ : state) {
    auto words = solver.Solve(data.boards.front());
    benchmark::DoNotOptimize(words);
  }
  state.SetItemsProcessed(state.iterations());
}
} // namespace

BENCHMARK_CAPTURE(BM_SolveRandomBoards, basic_english_2000, SMALL_DICTIONARY)
    ->Repetitions(10)
    ->ReportAggregatesOnly(true);
BENCHMARK_CAPTURE(BM_SolveRandomBoards, boggle_words_138735, LARGE_DICTIONARY)
    ->Repetitions(10)
    ->ReportAggregatesOnly(true);
BENCHMARK_CAPTURE(BM_SolveFixedBoard, basic_english_2000, SMALL_DICTIONARY)
    ->Repetitions(10)
    ->ReportAggregatesOnly(true);

BENCHMARK_MAIN();
