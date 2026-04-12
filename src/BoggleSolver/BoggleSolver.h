#ifndef BOGGLE_SOLVER_H
#define BOGGLE_SOLVER_H

// system includes
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// local includes
#include "BoggleBoard/BoggleBoard.h"
#include "Dictionary/Dictionary.h"

// Helper macros for bit shifting
constexpr uint32_t COUNT_SHIFT = 16;
constexpr uint32_t COUNT_MASK = 0xF;    // 4 bits (1111 in binary)
constexpr uint32_t INDEX_MASK = 0xFFFF; // 16 bits (all 1s)

/// @brief Helper method to precompute neighbour cells
static constexpr std::array<uint32_t, BOARD_SIZE> CalculateNeighbours() {
  std::array<uint32_t, BOARD_SIZE> adjacent_list{};

  // The 8 neighbour cells directions
  constexpr int32_t x_options[NEIGHBOUR_CELLS]{-1, 0, 1, -1, 1, -1, 0, 1};
  constexpr int32_t y_options[NEIGHBOUR_CELLS]{-1, -1, -1, 0, 0, 1, 1, 1};

  for (uint32_t i = 0; i < BOARD_SIZE; ++i) {
    const auto x_cell = static_cast<int32_t>(i % BOARD_WIDTH);
    const auto y_cell = static_cast<int32_t>(i / BOARD_WIDTH);

    uint32_t count = 0;
    uint32_t packed = 0;

    for (uint32_t delta = 0; delta < NEIGHBOUR_CELLS; ++delta) {
      const int32_t x_neighbour = x_cell + x_options[delta];
      const int32_t y_neighbour = y_cell + y_options[delta];

      if (x_neighbour >= 0 && x_neighbour < static_cast<int32_t>(BOARD_WIDTH) &&
          y_neighbour >= 0 &&
          y_neighbour < static_cast<int32_t>(BOARD_HEIGHT)) {
        const uint32_t neighbour = (y_neighbour * BOARD_WIDTH) + x_neighbour;

        // Set the bit corresponding to the neighbour index
        packed |= (1U << neighbour);
        count++;
      }
    }
    // Store the count in the upper bits and the bitmask in the lower bits
    adjacent_list[i] = (count << COUNT_SHIFT) | packed;
  }
  return adjacent_list;
}

/// @brief Solves a Boggle board for a given dictionary
class BoggleSolver {
public:
  /// @brief Precomputed adjacent cells at compile time
  static constexpr std::array<uint32_t, BOARD_SIZE> ADJACENT_CELLS =
      CalculateNeighbours();

  /// @brief Constructor of the Boggle Solver
  /// @param dictionary Shared pointer to the loaded dictionary
  explicit BoggleSolver(std::shared_ptr<Dictionary> dictionary);

  /// @brief Default destructor
  ~BoggleSolver() = default;

  // Delete copy constructor and copy assignment operator
  BoggleSolver(const BoggleSolver &) = delete;
  BoggleSolver &operator=(const BoggleSolver &) = delete;

  // Delete move constructor and move assignment operator
  BoggleSolver(BoggleSolver &&) = delete;
  BoggleSolver &operator=(BoggleSolver &&) = delete;

  /// @brief Returns words greater than 3 letters on the board
  /// @param board Flattened 1D boggle board
  /// @return A vector of words found on the board
  std::vector<std::string> Solve(const std::array<char, BOARD_SIZE> &board);

  /// @brief Helper method to extract the neighbor count from packed data
  /// @param packed The packed 32-bit neighbor data
  /// @return The number of neighbors
  static constexpr uint32_t GetNeighborCount(uint32_t packed) {
    return (packed >> COUNT_SHIFT) & COUNT_MASK;
  }

  /// @brief Returns precomputed adjacent cells
  /// @return Reference to the static constexpr array
  static const std::array<uint32_t, BOARD_SIZE> &GetAdjacentCells() {
    return ADJACENT_CELLS;
  }

private:
  std::shared_ptr<Dictionary> dictionary_;

  // Unique identifier each board to track found words (stop duplicates)
  uint32_t solve_id_ = 0;

  // Buffer to store the current word being built
  std::string current_word_buffer_;

  // Tracks each node index & sets the solve id (stops duplicate words)
  std::vector<uint32_t> word_has_been_found_;
};

#endif // BOGGLE_SOLVER_H
