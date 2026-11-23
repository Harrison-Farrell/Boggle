#include "BoggleBoard.h"

// system includes
#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <random>

BoggleBoard::BoggleBoard() : BoggleBoard(std::random_device{}()) {}

BoggleBoard::BoggleBoard(uint32_t seed) : generator_(seed) {
  std::iota(dice_order_.begin(), dice_order_.end(), uint8_t{0});
  Shuffle();
}

void BoggleBoard::Shuffle() {
  std::shuffle(dice_order_.begin(), dice_order_.end(), generator_);

  std::uniform_int_distribution<uint32_t> dist(0, DICE_FACES - 1);

  for (uint32_t i = 0; i < TOTAL_DICE; ++i) {
    const char face = STANDARD_DICE[dice_order_[i]][dist(generator_)];
    board_[i / BOARD_WIDTH][i % BOARD_WIDTH] = face;
  }
}

char BoggleBoard::GetCharAtIndex(uint32_t row, uint32_t col) const {
  if (row >= 0 && row < BOARD_WIDTH && col >= 0 && col < BOARD_HEIGHT) {
    return board_[row][col];
  }
  return '\0';
}

std::array<char, BOARD_SIZE> BoggleBoard::GetFlattenedBoard() const {
  std::array<char, BOARD_SIZE> flattened_board{};

  for (uint32_t row = 0; row < BOARD_WIDTH; ++row) {
    for (uint32_t col = 0; col < BOARD_HEIGHT; ++col) {
      flattened_board[(row * BOARD_WIDTH) + col] = board_[row][col];
    }
  }
  return flattened_board;
}

void BoggleBoard::PrintBoard() const {
  std::cout << "\n----- Boggle Board -----\n";
  for (uint32_t row = 0; row < BOARD_WIDTH; ++row) {
    for (uint32_t col = 0; col < BOARD_HEIGHT; ++col) {
      std::cout << "[ " << board_[row][col] << " ] ";
    }
    std::cout << '\n';
  }
  std::cout << "------------------------\n";
}
