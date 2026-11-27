#ifndef BOGGLE_SOLVER_H
#define BOGGLE_SOLVER_H

// system includes
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// local includes
#include "BoggleBoard/BoggleBoard.h"
#include "Dictionary/CompactTrie.h"
#include "Dictionary/Dictionary.h"

// Words shorter than this do not score in Boggle
constexpr uint32_t MIN_WORD_LENGTH = 3;

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
  /// @param dictionary Shared pointer to the loaded dictionary. A compact copy
  /// is taken, so words inserted afterwards are not found.
  explicit BoggleSolver(const std::shared_ptr<Dictionary> &dictionary);

  /// @brief Constructs a solver that shares an already built trie. Use this to
  /// run one solver per thread without copying the dictionary for each.
  /// @param trie Trie made by BuildTrie()
  explicit BoggleSolver(std::shared_ptr<const CompactTrie> trie);

  /// @brief Builds the trie used by the solver from a dictionary
  /// @param dictionary The loaded dictionary
  /// @return The trie, or nullptr if dictionary is nullptr
  static std::shared_ptr<const CompactTrie>
  BuildTrie(const std::shared_ptr<Dictionary> &dictionary);

  /// @brief Default destructor
  ~BoggleSolver() = default;

  // Delete copy constructor and copy assignment operator
  BoggleSolver(const BoggleSolver &) = delete;
  BoggleSolver &operator=(const BoggleSolver &) = delete;

  // Delete move constructor and move assignment operator
  BoggleSolver(BoggleSolver &&) = delete;
  BoggleSolver &operator=(BoggleSolver &&) = delete;

  /// @brief Returns owned words of at least 3 letters on the board
  /// @param board Flattened 1D boggle board
  /// @return A vector of words found on the board. Words made from fewer
  /// cells come first; Solve and SolveInto return the same order.
  std::vector<std::string> Solve(const std::array<char, BOARD_SIZE> &board);

  /// @brief Writes word views into a caller-owned buffer, retaining its
  /// capacity
  /// @param board Flattened 1D boggle board
  /// @param words Cleared before searching, including with a null trie
  /// @details Views refer to immutable trie text and remain valid while that
  /// CompactTrie is alive, including across later solves. Retain the shared
  /// trie if views must outlive this solver. No word text is copied.
  void SolveInto(const std::array<char, BOARD_SIZE> &board,
                 std::vector<std::string_view> &words);

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
  // Instantiated in the implementation for strings and string views, so both
  // APIs use the same search without an intermediate result vector.
  template <typename Word>
  void SolveImpl(const std::array<char, BOARD_SIZE> &board,
                 std::vector<Word> &found_words);

    // A path on the board that spells a prefix in the trie, packed into 64
    // bits:
    //   bits  0..31: trie node index
    //   bits 32..47: last cell index (0..15)
    //   bits 48..63: visited cell bitmask (16 bits)
    // Packing into uint64_t guarantees every candidate write is a single 64-bit
    // store instead of three narrow stores.
    using PackedPath = uint64_t;

    // Surviving non-dead path queued between the SIMD filter pass (Phase A) and
    // the branchless 2-slot expansion pass (Phase B).
    struct alignas(16) ActiveState {
      uint32_t mask;        // raw CompactTrie node mask (children + word bit)
      uint32_t first_child; // index of the node's first child
      uint32_t visited;     // 16-bit visited cell bitmask
      uint32_t keep;        // non-zero 16-bit bitmask of matching neighbours
    };

    // Read-only, so one trie can be shared by solvers on several threads
    std::shared_ptr<const CompactTrie> trie_;

    // Paths of the current length, and the paths one cell longer built from
    // them. Kept between solves so their capacity is reused.
    std::vector<PackedPath> level_;
    std::vector<PackedPath> next_level_;

    // Surviving paths (keep != 0) passed from Phase A to Phase B within a
    // level.
    std::vector<ActiveState> active_;

    // One bit per trie node, set during post-search deduplication so each found
    // word is listed once. Only the 64-bit blocks touched by found words are
    // cleared at the end of each solve.
    std::vector<uint64_t> found_word_bits_;

    // Candidate word nodes recorded branchlessly during search (duplicates are
    // filtered out after the search). Grows on demand if a board exceeds 1024
    // word hits.
    std::vector<uint32_t> found_word_nodes_;
  };

#endif // BOGGLE_SOLVER_H
