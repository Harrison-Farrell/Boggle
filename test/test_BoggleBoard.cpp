// system includes
#include <cctype>

// external includes
#include <gtest/gtest.h>

// local includes
#include "BoggleBoard/BoggleBoard.h"

namespace {
class BoggleBoardTest : public ::testing::Test {
protected:
  BoggleBoard board;
};
} // namespace

TEST_F(BoggleBoardTest, BoardIsAlphabeticalAndUppercase) {
  auto current_board = board.GetBoard();
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      const char cell = current_board[row][col];
      EXPECT_TRUE(std::isalpha(static_cast<unsigned char>(cell)));
      EXPECT_TRUE(std::isupper(static_cast<unsigned char>(cell)));
    }
  }
}

TEST_F(BoggleBoardTest, GetCharAtIndexValidBounds) {
  auto current_board = board.GetBoard();
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_EQ(board.GetCharAtIndex(row, col), current_board[row][col]);
    }
  }
}

TEST_F(BoggleBoardTest, GetCharAtIndexOutOfBounds) {
  EXPECT_EQ(board.GetCharAtIndex(-1, 0), '\0');
  EXPECT_EQ(board.GetCharAtIndex(0, -1), '\0');
  EXPECT_EQ(board.GetCharAtIndex(4, 0), '\0');
  EXPECT_EQ(board.GetCharAtIndex(0, 4), '\0');
  EXPECT_EQ(board.GetCharAtIndex(10, -5), '\0');
}

TEST_F(BoggleBoardTest, ShuffleChangesBoard) {
  auto initial_board = board.GetBoard();
  board.Shuffle();
  auto new_board = board.GetBoard();

  bool has_difference = false;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      if (initial_board[row][col] != new_board[row][col]) {
        has_difference = true;
        break;
      }
    }
    if (has_difference) {
      break;
    }
  }

  EXPECT_TRUE(has_difference);
}

TEST_F(BoggleBoardTest, GetFlattenedBoardMatchesBoard) {
  auto current_board = board.GetBoard();
  auto flattened = board.GetFlattenedBoard();

  EXPECT_EQ(flattened.size(), 16);
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_EQ(flattened[(row * BOARD_WIDTH) + col], current_board[row][col]);
    }
  }
}

TEST_F(BoggleBoardTest, GetFlattenedBoardAfterShuffle) {
  board.Shuffle();
  auto current_board = board.GetBoard();
  auto flattened = board.GetFlattenedBoard();

  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_EQ(flattened[(row * BOARD_WIDTH) + col], current_board[row][col]);
    }
  }
}