#ifndef BOGGLE_BOARD_H
#define BOGGLE_BOARD_H

// system includes
#include <array>
#include <bit>
#include <cstdint>
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
/// @details Handles the shuffling and generation of a random board
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
  /// @details Fully unrolled in-place Fisher-Yates shuffle and die-face roll
  /// consuming four inline 64-bit Xoshiro256++ outputs (32 random bytes: 15
  /// swaps and 16 face rolls) with zero divisions and zero out-of-line calls.
  inline void Shuffle() {
    auto step = [&]<uint32_t I>(uint64_t r_swap, uint64_t r_face) {
      const uint32_t swap =
          ((static_cast<uint32_t>(r_swap) & 0xFFU) * (I + 1U)) >> 8;
      const uint8_t die = dice_order_[swap];
      dice_order_[swap] = dice_order_[I];
      dice_order_[I] = die;
      const uint32_t face =
          ((static_cast<uint32_t>(r_face) & 0xFFU) * DICE_FACES) >> 8;
      board_[I] = STANDARD_DICE[die][face];
    };

    const uint64_t r0 = NextRandom64();
    const uint64_t r1 = NextRandom64();
    const uint64_t r2 = NextRandom64();
    const uint64_t r3 = NextRandom64();

    step.template operator()<15>(r0, r2);
    step.template operator()<14>(r0 >> 8, r2 >> 8);
    step.template operator()<13>(r0 >> 16, r2 >> 16);
    step.template operator()<12>(r0 >> 24, r2 >> 24);
    step.template operator()<11>(r0 >> 32, r2 >> 32);
    step.template operator()<10>(r0 >> 40, r2 >> 40);
    step.template operator()<9>(r0 >> 48, r2 >> 48);
    step.template operator()<8>(r0 >> 56, r2 >> 56);

    step.template operator()<7>(r1, r3);
    step.template operator()<6>(r1 >> 8, r3 >> 8);
    step.template operator()<5>(r1 >> 16, r3 >> 16);
    step.template operator()<4>(r1 >> 24, r3 >> 24);
    step.template operator()<3>(r1 >> 32, r3 >> 32);
    step.template operator()<2>(r1 >> 40, r3 >> 40);
    step.template operator()<1>(r1 >> 48, r3 >> 48);

    const uint32_t face0 =
        ((static_cast<uint32_t>(r3 >> 56) & 0xFFU) * DICE_FACES) >> 8;
    board_[0] = STANDARD_DICE[dice_order_[0]][face0];
  }

  /// @brief Gets the character at the given row and column
  /// @param row row index
  /// @param col column index
  /// @return character at the specified location, or '\0' if out of bounds
  char GetCharAtIndex(uint32_t row, uint32_t col) const;

  /// @brief Returns the 2D boggle board
  /// @return 4x4 array of characters representing the board
  std::array<std::array<char, BOARD_WIDTH>, BOARD_HEIGHT> GetBoard() const {
  return std::bit_cast<std::array<std::array<char, BOARD_WIDTH>, BOARD_HEIGHT>>(
      board_);
}

/// @brief Returns the boggle board in a flattened 1D array
/// @return array of 16 characters representing the board
std::array<char, BOARD_SIZE> GetFlattenedBoard() const { return board_; }

/// @brief Print an ASCII representation of the board to standard output
void PrintBoard() const;

private:
// Inline Xoshiro256++ (Blackman & Vigna): 32-byte state (vs 5,000 bytes for
// std::mt19937), period 2^256 - 1, passes BigCrush/PractRand, and inlines
// directly into Shuffle() with zero out-of-line function calls.
inline uint64_t NextRandom64() {
  const uint64_t result =
      std::rotl(rng_state_[0] + rng_state_[3], 23) + rng_state_[0];
  const uint64_t t = rng_state_[1] << 17;
  rng_state_[2] ^= rng_state_[0];
  rng_state_[3] ^= rng_state_[1];
  rng_state_[1] ^= rng_state_[2];
  rng_state_[0] ^= rng_state_[3];
  rng_state_[2] ^= t;
  rng_state_[3] = std::rotl(rng_state_[3], 45);
  return result;
}

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

// 32-byte Xoshiro256++ PRNG state seeded once via SplitMix64 on construction.
std::array<uint64_t, 4> rng_state_{};

// Order in which the dice are placed on the board, shuffled in place.
std::array<uint8_t, TOTAL_DICE> dice_order_{};

// Stored flat so Shuffle() writes directly to board_[i] without row/col math
// and GetFlattenedBoard() returns the 16-byte register value directly.
alignas(16) std::array<char, BOARD_SIZE> board_{};
}
;

#endif // BOGGLE_BOARD_H
