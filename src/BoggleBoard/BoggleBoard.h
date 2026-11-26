#ifndef BOGGLE_BOARD_H
#define BOGGLE_BOARD_H

// system includes
#include <array>
#include <bit>
#include <cstdint>
#include <random>
#include <string_view>

// global board variables
constexpr static const uint32_t BOARD_WIDTH = 4;
constexpr static const uint32_t BOARD_HEIGHT = 4;
constexpr static const uint32_t TOTAL_DICE = 16;
constexpr static const uint32_t DICE_FACES = 6;
constexpr static const uint32_t NEIGHBOUR_CELLS = 8;
constexpr static const uint32_t BOARD_SIZE = BOARD_WIDTH * BOARD_HEIGHT;
constexpr static const uint32_t MAX_WORD_LENGTH = 17;

/// @brief Represents a 4x4 Boggle board
/// @details Handles the shuffling and generation a random board
class BoggleBoard {
public:
  /// @brief Default constructor. Seeds the generator from std::random_device
  /// and shuffles the board
  BoggleBoard();

  /// @brief Constructs a board with a fixed seed, giving a reproducible
  /// sequence of boards (useful for benchmarks and tests)
  /// @param seed seed for the random number generator
  explicit BoggleBoard(uint32_t seed);

  /// @brief Default destructor for BoggleBoard
  ~BoggleBoard() = default;

  // Delete copy constructor and copy assignment operator
  BoggleBoard(const BoggleBoard &) = delete;
  BoggleBoard &operator=(const BoggleBoard &) = delete;

  // Delete move constructor and move assignment operator
  BoggleBoard(BoggleBoard &&) = delete;
  BoggleBoard &operator=(BoggleBoard &&) = delete;

  /// @brief Shuffles the dice and their faces to generate a new random board
  void Shuffle();

  /// @brief Gets the character at the given row and column
  /// @param row row index
  /// @param col column index
  /// @return character at the specified location, or '\0' if out of bounds
  char GetCharAtIndex(uint32_t row, uint32_t col) const;

  /// @brief Returns the 2D boggle board
  /// @return 4x4 array of characters representing the board
  std::array<std::array<char, BOARD_WIDTH>, BOARD_HEIGHT> GetBoard() const {
    return board_;
  };

  /// @brief Returns the boggle board in a flattened 1D array
  /// @return array of 16 characters representing the board
  std::array<char, BOARD_SIZE> GetFlattenedBoard() const {
    return std::bit_cast<std::array<char, BOARD_SIZE>>(board_);
  }

  /// @brief Print an ASCII representation of the board to standard output
  void PrintBoard() const;

private:
  // Stored as a flat 2D table (aligned to 8 bytes per die) rather than
  // string_views to avoid pointer indirection on every die face lookup.
  static constexpr std::array<std::array<char, 8>, TOTAL_DICE> STANDARD_DICE = {{
      {'A', 'W', 'O', 'T', 'O', 'T', 0, 0},
      {'B', 'O', 'A', 'J', 'B', 'O', 0, 0},
      {'H', 'N', 'Z', 'N', 'H', 'L', 0, 0},
      {'I', 'S', 'T', 'O', 'E', 'S', 0, 0},
      {'T', 'S', 'D', 'I', 'Y', 'T', 0, 0},
      {'P', 'S', 'C', 'A', 'O', 'H', 0, 0},
      {'T', 'Y', 'T', 'L', 'R', 'E', 0, 0},
      {'E', 'R', 'X', 'I', 'D', 'L', 0, 0},
      {'F', 'S', 'A', 'F', 'P', 'K', 0, 0},
      {'Q', 'U', 'I', 'H', 'N', 'M', 0, 0},
      {'S', 'N', 'E', 'I', 'U', 'E', 0, 0},
      {'A', 'E', 'A', 'N', 'G', 'E', 0, 0},
      {'R', 'V', 'T', 'W', 'E', 'H', 0, 0},
      {'T', 'M', 'U', 'O', 'I', 'C', 0, 0},
      {'W', 'E', 'E', 'N', 'G', 'H', 0, 0},
      {'L', 'Y', 'V', 'R', 'D', 'E', 0, 0},
  }};

  // Seeded once on construction. Re-seeding on every shuffle is expensive.
  std::mt19937 generator_;

  // Order in which the dice are placed on the board, shuffled in place
  std::array<uint8_t, TOTAL_DICE> dice_order_{};

  std::array<std::array<char, BOARD_WIDTH>, BOARD_HEIGHT> board_{};
};

#endif // BOGGLE_BOARD_H
