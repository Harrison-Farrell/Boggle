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
  // Perform an in-place Fisher-Yates shuffle and select each die's face in a
  // single pass. Each 32-bit RNG output supplies four 8-bit fixed-point
  // multiplications (two die swaps and two face rolls), requiring only 8 RNG
  // draws and zero integer divisions per board.
  for (uint32_t i = TOTAL_DICE - 1; i >= 2; i -= 2) {
    const uint32_t r = generator_();

    const uint32_t swap0 = ((r & 0xFFU) * (i + 1U)) >> 8;
    const uint8_t die0 = dice_order_[swap0];
    dice_order_[swap0] = dice_order_[i];
    dice_order_[i] = die0;
    const uint32_t face0 = (((r >> 8) & 0xFFU) * DICE_FACES) >> 8;
    board_[i / BOARD_WIDTH][i % BOARD_WIDTH] = STANDARD_DICE[die0][face0];

    const uint32_t swap1 = (((r >> 16) & 0xFFU) * i) >> 8;
    const uint8_t die1 = dice_order_[swap1];
    dice_order_[swap1] = dice_order_[i - 1];
    dice_order_[i - 1] = die1;
    const uint32_t face1 = ((r >> 24) * DICE_FACES) >> 8;
    board_[(i - 1) / BOARD_WIDTH][(i - 1) % BOARD_WIDTH] =
        STANDARD_DICE[die1][face1];
  }
  const uint32_t r = generator_();
  const uint32_t swap1 = ((r & 0xFFU) * 2U) >> 8;
  const uint8_t die1 = dice_order_[swap1];
  dice_order_[swap1] = dice_order_[1];
  dice_order_[1] = die1;
  board_[0][1] = STANDARD_DICE[die1][(((r >> 8) & 0xFFU) * DICE_FACES) >> 8];
  board_[0][0] =
      STANDARD_DICE[dice_order_[0]][(((r >> 16) & 0xFFU) * DICE_FACES) >> 8];
}

char BoggleBoard::GetCharAtIndex(uint32_t row, uint32_t col) const {
  if (row >= 0 && row < BOARD_WIDTH && col >= 0 && col < BOARD_HEIGHT) {
    return board_[row][col];
  }
  return '\0';
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
