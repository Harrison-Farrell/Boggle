// system includes
#include <memory>
#include <string>

// external includes
#include <benchmark/benchmark.h>

// local includes
#include "BoggleBoard/BoggleBoard.h"
#include "BoggleSolver/BoggleSolver.h"
#include "Dictionary/Dictionary.h"

namespace {
// A fixture to load the dictionary once per benchmark run
class BoggleFixture : public benchmark::Fixture {
  std::shared_ptr<Dictionary> dictionary_ = nullptr;
  std::shared_ptr<BoggleBoard> board_;

public:
  std::shared_ptr<Dictionary> GetDictionary() const { return dictionary_; }
  std::shared_ptr<BoggleBoard> GetBoard() const { return board_; }

  void SetUp(const ::benchmark::State &) override {
    dictionary_ = ReadWordsFromFile("basic_english_words_2000.txt");

    board_ = std::make_unique<BoggleBoard>();
    board_->Shuffle();
  }

  void TearDown(const ::benchmark::State &state) override { (void)state; }
};
} // namespace

namespace {
BENCHMARK_DEFINE_F(BoggleFixture,
                   BM_SolveRandomBoard)(benchmark::State &state) {
  BoggleSolver solver(GetDictionary());
  for (auto _ : state) {
    // Run the solver iteratively
    auto words = solver.Solve(GetBoard()->GetFlattenedBoard());
    benchmark::DoNotOptimize(words);
  }
}
} // namespace

BENCHMARK_REGISTER_F(BoggleFixture, BM_SolveRandomBoard)->Repetitions(50);

BENCHMARK_MAIN();
