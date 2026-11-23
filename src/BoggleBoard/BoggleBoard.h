#ifndef BOGGLE_BOARD_H
#define BOGGLE_BOARD_H

// system includes
#include <array>
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
  std::array<char, BOARD_SIZE> GetFlattenedBoard() const;

  /// @brief Print an ASCII representation of the board to standard output
  void PrintBoard() const;

private:
  static constexpr std::array<std::string_view, TOTAL_DICE> STANDARD_DICE = {
      "AWOTOT", "BOAJBO", "HNZNHL", "ISTOES", "TSDIYT", "PSCAOH",
      "TYTLRE", "ERXIDL", "FSAFPK", "QUIHNM", "SNEIUE", "AEANGE",
      "RVTWEH", "TMUOIC", "WEENGH", "LYVRDE"};

  // Seeded once on construction. Re-seeding on every shuffle is expensive.
  std::mt19937 generator_;

  // Order in which the dice are placed on the board, shuffled in place
  std::array<uint8_t, TOTAL_DICE> dice_order_{};

  std::array<std::array<char, BOARD_WIDTH>, BOARD_HEIGHT> board_{};
};

#endif // BOGGLE_BOARD_H
