#include "BoggleBoard.h"

// system includes
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>

BoggleBoard::BoggleBoard() : BoggleBoard(std::random_device{}()) {}

  BoggleBoard::BoggleBoard(uint32_t seed) {
    // Expand the 32-bit seed into 256 well-mixed state bits via SplitMix64.
    uint64_t z_state = seed;
    for (uint64_t &word : rng_state_) {
      z_state += 0x9e3779b97f4a7c15ULL;
      uint64_t z = z_state;
      z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
      z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
      word = z ^ (z >> 31);
    }
    std::iota(dice_order_.begin(), dice_order_.end(), uint8_t{0});
    Shuffle();
  }

  char BoggleBoard::GetCharAtIndex(uint32_t row, uint32_t col) const {
      if (row < BOARD_HEIGHT && col < BOARD_WIDTH) {
        return board_[(row * BOARD_WIDTH) + col];
      }
      return '\0';
    }

    void BoggleBoard::PrintBoard() const {
      std::cout << "\n----- Boggle Board -----\n";
          for (uint32_t row = 0; row < BOARD_HEIGHT; ++row) {
            for (uint32_t col = 0; col < BOARD_WIDTH; ++col) {
              std::cout << "[ " << board_[(row * BOARD_WIDTH) + col] << " ] ";
            }
            std::cout << '\n';
          }
          std::cout << "------------------------\n";
        }
