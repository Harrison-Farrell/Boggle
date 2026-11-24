#include "BoggleSolver.h"

// system includes
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(__SSSE3__)
#include <tmmintrin.h>
#endif

// local includes
#include "BoggleBoard/BoggleBoard.h"
#include "Dictionary/CompactTrie.h"
#include "Dictionary/Dictionary.h"

BoggleSolver::BoggleSolver(const std::shared_ptr<Dictionary> &dictionary)
    : BoggleSolver(BuildTrie(dictionary)) {}

BoggleSolver::BoggleSolver(std::shared_ptr<const CompactTrie> trie)
    : trie_(std::move(trie)) {
  if (trie_) {
    word_has_been_found_.resize(trie_->GetTotalNodes(), 0);
  }
}

std::shared_ptr<const CompactTrie>
BoggleSolver::BuildTrie(const std::shared_ptr<Dictionary> &dictionary) {
  if (!dictionary) {
    return nullptr;
  }
  return std::make_shared<const CompactTrie>(*dictionary, MIN_WORD_LENGTH);
}

std::vector<std::string>
BoggleSolver::Solve(const std::array<char, BOARD_SIZE> &board) {
  std::vector<std::string> found_words;
  if (!trie_) [[unlikely]] {
    return found_words;
  }
  const CompactTrie &trie = *trie_;

  // Enables tracking of words found on this specific board
  // This is used to prevent finding the same word twice
  // Can call `Solve(board)` on many boards and it will still work
  // as the solve_id_ increments for each board
  solve_id_++;
  if (solve_id_ == 0) [[unlikely]] {
    // Reset tracking on wrap-around (unsigned int)
    std::ranges::fill(word_has_been_found_, 0);
    solve_id_ = 1;
  }

  // Convert the board to letter indices once. Anything that is not a letter
  // maps to a letter that never has a child, so it can never be part of a word.
  std::array<uint32_t, BOARD_SIZE> letters{};
#if defined(__SSSE3__)
  // Each cell selects the byte of a trie mask containing its letter, then
  // tests the letter's bit. Invalid cells have a zero bit and never match.
  std::array<uint8_t, BOARD_SIZE> letter_bytes{};
  std::array<uint8_t, BOARD_SIZE> letter_bits{};
#endif
  for (uint32_t cell = 0; cell < BOARD_SIZE; ++cell) {
    const int upper = std::toupper(static_cast<unsigned char>(board[cell]));
    letters[cell] = (upper >= 'A' && upper <= 'Z')
                        ? static_cast<uint32_t>(upper - 'A')
                        : CompactTrie::INVALID_LETTER;
#if defined(__SSSE3__)
    if (letters[cell] != CompactTrie::INVALID_LETTER) {
      letter_bytes[cell] = static_cast<uint8_t>(letters[cell] / 8);
      letter_bits[cell] = static_cast<uint8_t>(1U << (letters[cell] % 8));
    }
#endif
  }

#if defined(__SSSE3__)
  static_assert(BOARD_SIZE == 16, "SIMD letter filtering requires 16 cells");
  const __m128i byte_selectors =
      _mm_loadu_si128(reinterpret_cast<const __m128i *>(letter_bytes.data()));
  const __m128i bit_selectors =
      _mm_loadu_si128(reinterpret_cast<const __m128i *>(letter_bits.data()));
#endif

  // The trie already reads 'Q' as "QU" and only marks words of at least
  // MIN_WORD_LENGTH letters, so the search does not need to handle either.
  auto depth_first_search = [&](this auto &&self, uint32_t current_grid_index,
                                uint32_t visited_mask,
                                int32_t current_node_index) -> void {
    if (trie.IsWord(current_node_index) &&
        word_has_been_found_[current_node_index] != solve_id_) {
      // mark the word as found for this board
      word_has_been_found_[current_node_index] = solve_id_;
      found_words.emplace_back(trie.GetWord(current_node_index));
    }

    // No longer word starts with this prefix
    const uint32_t children = trie.GetChildrenMask(current_node_index);
    if (children == 0) {
      return;
    }

    visited_mask |= (1U << current_grid_index);

    // Neighbours of the current cell that are not on the current path
    uint32_t bitmask =
        ADJACENT_CELLS[current_grid_index] & INDEX_MASK & ~visited_mask;

#if defined(__SSSE3__)
    // Project all outgoing trie letters onto the board's 16 cells at once.
    const __m128i mask_bytes = _mm_set1_epi32(static_cast<int32_t>(children));
    const __m128i matching_bits = _mm_and_si128(
        _mm_shuffle_epi8(mask_bytes, byte_selectors), bit_selectors);
    const uint32_t matching_cells = static_cast<uint32_t>(
        ~_mm_movemask_epi8(_mm_cmpeq_epi8(matching_bits, _mm_setzero_si128())));
    bitmask &= matching_cells;
#endif

    while (bitmask > 0) {
      const auto next_index = static_cast<uint32_t>(std::countr_zero(bitmask));

      const int32_t next_node =
          trie.GetNextNodeIndex(current_node_index, letters[next_index]);
      if (next_node != -1) {
        self(next_index, visited_mask, next_node);
      }

      // Clear the lowest set bit
      bitmask &= (bitmask - 1);
    }
  };

  // Start a search from every letter on the board
  for (uint32_t current_grid_index = 0; current_grid_index < BOARD_SIZE;
       ++current_grid_index) {
    const int32_t current_node_index = trie.GetNextNodeIndex(
        CompactTrie::GetRootIndex(), letters[current_grid_index]);

    if (current_node_index != -1) {
      depth_first_search(current_grid_index, 0, current_node_index);
    }
  }

  return found_words;
}
