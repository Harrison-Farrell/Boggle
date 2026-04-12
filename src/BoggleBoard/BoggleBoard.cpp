#include "BoggleBoard.h"

// system includes
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <string>
#include <vector>

const std::vector<std::string> BoggleBoard::STANDARD_DICE = {
    "AWOTOT", "BOAJBO", "HNZNHL", "ISTOES", "TSDIYT", "PSCAOH",
    "TYTLRE", "ERXIDL", "FSAFPK", "QUIHNM", "SNEIUE", "AEANGE",
    "RVTWEH", "TMUOIC", "WEENGH", "LYVRDE"};

BoggleBoard::BoggleBoard() : board_{} { Shuffle(); }

void BoggleBoard::Shuffle() {
  std::random_device seed;
  std::mt19937 generator(seed());

  std::vector<std::string> dice = STANDARD_DICE;
  std::shuffle(dice.begin(), dice.end(), generator);

  std::uniform_int_distribution<int> dist(0, DICE_FACES - 1);

  for (uint32_t i = 0; i < TOTAL_DICE; ++i) {
    const char face = dice[i][dist(generator)];
    board_[i / BOARD_WIDTH][i % BOARD_HEIGHT] = face;
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
