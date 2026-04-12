// system includes
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

// external includes
#include <gtest/gtest.h>

// local includes
#include "BoggleBoard/BoggleBoard.h"
#include "BoggleSolver/BoggleSolver.h"
#include "Dictionary/Dictionary.h"

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