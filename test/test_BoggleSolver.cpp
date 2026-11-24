// system includes
#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// external includes
#include <gtest/gtest.h>

// local includes
#include "BoggleBoard/BoggleBoard.h"
#include "BoggleSolver/BoggleSolver.h"
#include "Dictionary/Dictionary.h"

namespace {
// Independent word-by-word search: no trie, packed adjacency, or SIMD masks.
// This also checks that separate paths do not accidentally share visited cells.
bool CanSpell(const std::array<char, BOARD_SIZE> &board,
              std::string_view word) {
  auto search = [&](this auto &&self, uint32_t cell, size_t offset,
                    std::array<bool, BOARD_SIZE> visited) -> bool {
    if (visited[cell]) {
      return false;
    }
    const char letter = static_cast<char>(
        std::toupper(static_cast<unsigned char>(board[cell])));
    const std::string_view face =
        letter == 'Q' ? "QU" : std::string_view(&letter, 1);
    if (word.substr(offset, face.size()) != face) {
      return false;
    }
    offset += face.size();
    if (offset == word.size()) {
      return true;
    }
    visited[cell] = true;
    const int row = static_cast<int>(cell / BOARD_WIDTH);
    const int col = static_cast<int>(cell % BOARD_WIDTH);
    for (int y = std::max(0, row - 1);
         y <= std::min(static_cast<int>(BOARD_HEIGHT) - 1, row + 1); ++y) {
      for (int x = std::max(0, col - 1);
           x <= std::min(static_cast<int>(BOARD_WIDTH) - 1, col + 1); ++x) {
        if (self(static_cast<uint32_t>(y) * BOARD_WIDTH + x, offset, visited)) {
          return true;
        }
      }
    }
    return false;
  };
  for (uint32_t cell = 0; cell < BOARD_SIZE; ++cell) {
    if (search(cell, 0, {})) {
      return true;
    }
  }
  return false;
}
} // namespace

TEST(BoggleSolverLogicTest, MatchesIndependentSearchAcrossVariedBoards) {
  // Include every byte of the 26-bit child mask, QU, short words, prefixes,
  // repeated letters and words that require reusing a cell to be spelled.
  std::vector<std::string> vocabulary = {
      "AA",  "AAA",  "AAAA",  "AAAAA", "AEA", "AEAE", "AEAEA", "AET",  "AETS",
      "QUA", "QUAT", "QUATE", "QUQUA", "QAT", "ZOO",  "ZOOS",  "YET",  "JET",
      "KEY", "KEYS", "VET",   "VETS",  "TEA", "TEAS", "SEA",   "SEAT", "TEST"};
  std::mt19937 random(2024);
  constexpr std::string_view alphabet = "AEIJKOPQSTUVXYZ";
  for (uint32_t i = 0; i < 2000; ++i) {
    std::string word;
    const uint32_t cells = 2 + random() % 6;
    for (uint32_t j = 0; j < cells; ++j) {
      const char letter = alphabet[random() % alphabet.size()];
      word += letter;
      if (letter == 'Q') {
        word += 'U';
      }
    }
    vocabulary.push_back(std::move(word));
  }
  std::ranges::sort(vocabulary);
  vocabulary.erase(std::unique(vocabulary.begin(), vocabulary.end()),
                   vocabulary.end());
  auto dictionary = std::make_shared<Dictionary>();
  for (const auto &word : vocabulary) {
    ASSERT_TRUE(dictionary->Insert(word));
  }
  BoggleSolver solver(dictionary);
  std::vector<std::string_view> views;
  BoggleBoard standard_board(2024);
  for (uint32_t sample = 0; sample < 128; ++sample) {
    auto board = standard_board.GetFlattenedBoard();
    standard_board.Shuffle();
    if (sample < 64) {
      for (char &letter : board) {
        letter = alphabet[random() % alphabet.size()];
      }
    }
    if (sample == 0) {
      board.fill('A');
    } else if (sample == 1) {
      board = {'a', 'e', '?', 'q', 'z', '\0', 't', 'a',
               'Q', 'u', 'A', 'e', 'i', 'x',  'y', 's'};
    } else if (sample == 2) {
      board.fill('#');
      board[0] = 'A';
      board[1] = 'E'; // AEA cannot reuse the only A.
    }
    std::vector<std::string> expected;
    for (const auto &word : vocabulary) {
      if (word.size() >= MIN_WORD_LENGTH && CanSpell(board, word)) {
        expected.push_back(word);
      }
    }
    auto actual = solver.Solve(board);
    solver.SolveInto(board, views);
    EXPECT_EQ(std::vector<std::string>(views.begin(), views.end()), actual)
        << "View order and contents on board sample " << sample;
    std::ranges::sort(actual);
    EXPECT_EQ(actual, expected) << "Board sample " << sample;
  }
}

TEST(BoggleSolverLogicTest, SolveIntoReusesCapacityAndClearsResults) {
  auto dictionary = std::make_shared<Dictionary>();
  dictionary->Insert("TEA");
  BoggleSolver solver(dictionary);
  const std::array<char, BOARD_SIZE> board = {'t', 'e', 'a', '?', 'T', '#',
                                              '#', '#', '#', '#', '#', '#',
                                              '#', '#', '#', '#'};
  std::vector<std::string_view> words;
  words.reserve(64);
  const auto capacity = words.capacity();
  const auto *storage = words.data();
  for (int repeat = 0; repeat < 3; ++repeat) {
    solver.SolveInto(board, words);
    EXPECT_EQ(words, std::vector<std::string_view>{"TEA"});
    EXPECT_EQ(words.capacity(), capacity);
    EXPECT_EQ(words.data(), storage);
    EXPECT_EQ(solver.Solve(board), std::vector<std::string>{"TEA"});
  }
  std::array<char, BOARD_SIZE> empty{};
  solver.SolveInto(empty, words);
  EXPECT_TRUE(words.empty());
  EXPECT_EQ(words.capacity(), capacity);

  words.emplace_back("stale result");
  BoggleSolver null_solver(std::shared_ptr<Dictionary>{});
  null_solver.SolveInto(board, words);
  EXPECT_TRUE(words.empty());
  EXPECT_EQ(words.capacity(), capacity);
}

TEST(BoggleSolverLogicTest, ViewsBorrowTrieTextAndSurviveLaterSolves) {
  auto dictionary = std::make_shared<Dictionary>();
  constexpr std::string_view long_word = "ABCDEFGHIJKLMNOP";
  dictionary->Insert(std::string(long_word));
  dictionary->Insert("ABC");
  const auto trie = BoggleSolver::BuildTrie(dictionary);
  int32_t node = CompactTrie::GetRootIndex();
  for (const char letter : long_word) {
    node = trie->GetNextNodeIndex(node, static_cast<uint32_t>(letter - 'A'));
  }
  ASSERT_NE(node, -1);
  std::vector<std::string_view> saved;
  {
    BoggleSolver solver(trie);
    const std::array<char, BOARD_SIZE> board = {'A', 'B', 'C', 'D', 'H', 'G',
                                                'F', 'E', 'I', 'J', 'K', 'L',
                                                'P', 'O', 'N', 'M'};
    std::vector<std::string_view> words;
    solver.SolveInto(board, words);
    const auto found = std::ranges::find(words, long_word);
    ASSERT_NE(found, words.end());
    EXPECT_EQ(found->data(), trie->GetWord(node).data());
    saved = words;
    std::array<char, BOARD_SIZE> empty{};
    solver.SolveInto(empty, words);
    EXPECT_TRUE(words.empty());
    EXPECT_EQ(solver.Solve(board),
              (std::vector<std::string>{"ABC", std::string(long_word)}));
  }
  // The retained shared trie owns the text after the solver is destroyed.
  EXPECT_EQ(saved, (std::vector<std::string_view>{"ABC", long_word}));
}

TEST(BoggleSolverLogicTest, FindsWordsInFixedBoard) {
  auto dict = std::make_shared<Dictionary>();
  // Insert words to be found
  dict->Insert("TEST");
  dict->Insert("TEA");
  dict->Insert("EAT");
  dict->Insert("QUEEN");
  dict->Insert("QUIET");
  dict->Insert("STAR");
  dict->Insert("ART");
  dict->Insert("RAT");
  dict->Insert("ARE");
  dict->Insert("TA");
  dict->Insert("AT");
  dict->Insert("AR");
  dict->Insert("IT");

  BoggleSolver solver(dict);

  // Create board
  // T E S A
  // T A R T
  // Q I I E
  // N E T S
  const std::array<char, BOARD_SIZE> board = {'T', 'E', 'S', 'A', 'T', 'A',
                                              'R', 'T', 'Q', 'I', 'I', 'E',
                                              'N', 'E', 'T', 'S'};

  auto words = solver.Solve(board);

  // should be found
  EXPECT_TRUE(std::ranges::find(words, "TEST") != words.end());
  EXPECT_TRUE(std::ranges::find(words, "ART") != words.end());
  EXPECT_TRUE(std::ranges::find(words, "RAT") != words.end());
  EXPECT_TRUE(std::ranges::find(words, "ARE") != words.end());
  EXPECT_TRUE(std::ranges::find(words, "TEA") != words.end());
  EXPECT_TRUE(std::ranges::find(words, "EAT") != words.end());
  EXPECT_TRUE(std::ranges::find(words, "STAR") != words.end());
  EXPECT_TRUE(std::ranges::find(words, "QUIET") != words.end());
  // should not be found
  EXPECT_FALSE(std::ranges::find(words, "QUEEN") != words.end());
  EXPECT_FALSE(std::ranges::find(words, "AT") != words.end());
  EXPECT_FALSE(std::ranges::find(words, "TA") != words.end());
  EXPECT_FALSE(std::ranges::find(words, "AR") != words.end());
  EXPECT_FALSE(std::ranges::find(words, "IT") != words.end());
}

TEST(BoggleSolverLogicTest, WordReachableTwiceIsReportedOnce) {
  auto dict = std::make_shared<Dictionary>();
  dict->Insert("TEA");

  BoggleSolver solver(dict);

  // "TEA" can be traced from either T
  // T E A X
  // T X X X
  // X X X X
  // X X X X
  const std::array<char, BOARD_SIZE> board = {'T', 'E', 'A', 'X', 'T', 'X',
                                              'X', 'X', 'X', 'X', 'X', 'X',
                                              'X', 'X', 'X', 'X'};

  const auto words = solver.Solve(board);
  EXPECT_EQ(words, std::vector<std::string>{"TEA"});

  // A second solve of the same board finds it again
  EXPECT_EQ(solver.Solve(board), std::vector<std::string>{"TEA"});
}

TEST(BoggleSolverLogicTest, CellsThatAreNotLettersAreSkipped) {
  auto dict = std::make_shared<Dictionary>();
  dict->Insert("TEA");
  dict->Insert("T1A");

  BoggleSolver solver(dict);

  const std::array<char, BOARD_SIZE> board = {'t', 'e', 'a', '1', '\0', '?',
                                              '#', ' ', '[', '@', '{',  '`',
                                              '~', '0', '9', '_'};

  EXPECT_EQ(solver.Solve(board), std::vector<std::string>{"TEA"});
}

TEST(BoggleSolverLogicTest, NullDictionaryFindsNothing) {
  BoggleSolver solver(std::shared_ptr<Dictionary>{});
  std::array<char, BOARD_SIZE> board{};
  board.fill('A');
  EXPECT_TRUE(solver.Solve(board).empty());
}

TEST(BoggleSolverLogicTest, SolversCanShareATrie) {
  auto dict = std::make_shared<Dictionary>();
  dict->Insert("TEA");
  const auto trie = BoggleSolver::BuildTrie(dict);

  BoggleSolver first(trie);
  BoggleSolver second(trie);

  const std::array<char, BOARD_SIZE> board = {'T', 'E', 'A', 'X', 'X', 'X',
                                              'X', 'X', 'X', 'X', 'X', 'X',
                                              'X', 'X', 'X', 'X'};
  EXPECT_EQ(first.Solve(board), std::vector<std::string>{"TEA"});
  EXPECT_EQ(second.Solve(board), std::vector<std::string>{"TEA"});
}

TEST(BoggleSolverTest, PrecomputeCellsCorrectNeighbour) {
  auto dict = std::make_shared<Dictionary>();
  const BoggleSolver solver(dict);

  // Test corner cells (should have 3 neighbour cells)
  EXPECT_EQ(BoggleSolver::GetNeighborCount(solver.GetAdjacentCells()[0]), 3);
  EXPECT_EQ(BoggleSolver::GetNeighborCount(solver.GetAdjacentCells()[3]), 3);
  EXPECT_EQ(BoggleSolver::GetNeighborCount(solver.GetAdjacentCells()[12]), 3);
  EXPECT_EQ(BoggleSolver::GetNeighborCount(solver.GetAdjacentCells()[15]), 3);

  // Test edge cells (should have 5 neighbour cells)
  EXPECT_EQ(BoggleSolver::GetNeighborCount(solver.GetAdjacentCells()[1]), 5);
  EXPECT_EQ(BoggleSolver::GetNeighborCount(solver.GetAdjacentCells()[4]), 5);

  // Test center cells (should have 8 neighbour cells)
  EXPECT_EQ(BoggleSolver::GetNeighborCount(solver.GetAdjacentCells()[5]), 8);
  EXPECT_EQ(BoggleSolver::GetNeighborCount(solver.GetAdjacentCells()[10]), 8);
}

TEST(BoggleSolverTest, AdjacentCellsCorrectIndices) {
  auto dict = std::make_shared<Dictionary>();
  const BoggleSolver solver(dict);

  auto has_neighbor = [](uint32_t packed, uint32_t neighbor_index) {
    return (packed & (1U << neighbor_index)) != 0;
  };

  // Cell 0 (top-left) should have neighbors at 1, 4, 5
  uint32_t cell0 = solver.GetAdjacentCells()[0];
  EXPECT_TRUE(has_neighbor(cell0, 1));
  EXPECT_TRUE(has_neighbor(cell0, 4));
  EXPECT_TRUE(has_neighbor(cell0, 5));
  // Total bits set should be 3
  EXPECT_EQ(BoggleSolver::GetNeighborCount(cell0), 3);

  // Cell 5 (center) should have neighbors at 0, 1, 2, 4, 6, 8, 9, 10
  uint32_t cell5 = solver.GetAdjacentCells()[5];
  EXPECT_TRUE(has_neighbor(cell5, 0));
  EXPECT_TRUE(has_neighbor(cell5, 1));
  EXPECT_TRUE(has_neighbor(cell5, 2));
  EXPECT_TRUE(has_neighbor(cell5, 4));
  EXPECT_TRUE(has_neighbor(cell5, 6));
  EXPECT_TRUE(has_neighbor(cell5, 8));
  EXPECT_TRUE(has_neighbor(cell5, 9));
  EXPECT_TRUE(has_neighbor(cell5, 10));
  // Total bits set should be 8
  EXPECT_EQ(BoggleSolver::GetNeighborCount(cell5), 8);
}
