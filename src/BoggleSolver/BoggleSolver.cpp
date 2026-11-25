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
  SolveImpl(board, found_words);
  return found_words;
}

void BoggleSolver::SolveInto(const std::array<char, BOARD_SIZE> &board,
                             std::vector<std::string_view> &words) {
  SolveImpl(board, words);
}

namespace {
// The neighbours of each cell in ascending order, padded with BOARD_SIZE.
// Neighbour masks never have bit BOARD_SIZE set, so padding is never kept.
constexpr std::array<std::array<uint8_t, NEIGHBOUR_CELLS>, BOARD_SIZE>
    NEIGHBOUR_LIST = [] {
      std::array<std::array<uint8_t, NEIGHBOUR_CELLS>, BOARD_SIZE> list{};
      for (uint32_t cell = 0; cell < BOARD_SIZE; ++cell) {
        uint32_t neighbours = BoggleSolver::ADJACENT_CELLS[cell] & INDEX_MASK;
        for (auto &slot : list[cell]) {
          slot = neighbours != 0
                     ? static_cast<uint8_t>(std::countr_zero(neighbours))
                     : static_cast<uint8_t>(BOARD_SIZE);
          neighbours &= neighbours - 1;
        }
      }
      return list;
    }();
} // namespace

template <typename Word>
void BoggleSolver::SolveImpl(const std::array<char, BOARD_SIZE> &board,
                             std::vector<Word> &found_words) {
  found_words.clear();
  if (!trie_) [[unlikely]] {
    return;
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
  // For each cell, the letters alphabetically before its letter. Counting the
  // children among them gives the cell's child in a node. The extra entry is
  // for the padding in NEIGHBOUR_LIST.
  std::array<uint32_t, BOARD_SIZE + 1> lower_letters{};
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
    if (letters[cell] != CompactTrie::INVALID_LETTER) {
      lower_letters[cell] = (1U << letters[cell]) - 1;
#if defined(__SSSE3__)
      letter_bytes[cell] = static_cast<uint8_t>(letters[cell] / 8);
      letter_bits[cell] = static_cast<uint8_t>(1U << (letters[cell] % 8));
#endif
    }
  }

#if defined(__SSSE3__)
  static_assert(BOARD_SIZE == 16, "SIMD letter filtering requires 16 cells");
  const __m128i byte_selectors =
      _mm_loadu_si128(reinterpret_cast<const __m128i *>(letter_bytes.data()));
  const __m128i bit_selectors =
      _mm_loadu_si128(reinterpret_cast<const __m128i *>(letter_bits.data()));
#endif

  // The board cells whose letter is one of a node's children
  auto matching_cells = [&](uint32_t children) -> uint32_t {
#if defined(__SSSE3__)
    // Project all outgoing trie letters onto the board's 16 cells at once.
    const __m128i mask_bytes = _mm_set1_epi32(static_cast<int32_t>(children));
    const __m128i matching_bits = _mm_and_si128(
        _mm_shuffle_epi8(mask_bytes, byte_selectors), bit_selectors);
    return INDEX_MASK & ~static_cast<uint32_t>(_mm_movemask_epi8(
                            _mm_cmpeq_epi8(matching_bits, _mm_setzero_si128())));
#else
    // INVALID_LETTER is above every child bit, so invalid cells never match
    uint32_t cells = 0;
    for (uint32_t cell = 0; cell < BOARD_SIZE; ++cell) {
      cells |= ((children >> letters[cell]) & 1U) << cell;
    }
    return cells;
#endif
  };

  // The search grows every path by one cell per pass (breadth first) rather
  // than following one path to its end (depth first). The paths of one length
  // sit in a flat array, and each writes a candidate for all eight neighbour
  // slots but only keeps those that extend a prefix. How many neighbours
  // match is hard to predict, so this avoids a mispredicted branch for most
  // paths, and the trie reads for one length do not depend on each other.
  //
  // The trie already reads 'Q' as "QU" and only marks words of at least
  // MIN_WORD_LENGTH letters, so the search does not need to handle either.
  const int32_t root = CompactTrie::GetRootIndex();
  const uint32_t root_children = trie.GetChildrenMask(root);
  const uint32_t root_first_child = trie.GetFirstChildIndex(root);

  if (level_.size() < BOARD_SIZE) {
    level_.resize(BOARD_SIZE);
  }
  size_t level_size = 0;
  for (uint32_t starts = matching_cells(root_children); starts != 0;
       starts &= starts - 1) {
    const auto cell = static_cast<uint32_t>(std::countr_zero(starts));
    level_[level_size++] = {
        root_first_child + std::popcount(root_children & lower_letters[cell]),
        cell, 1U << cell};
  }

  while (level_size > 0) {
    // Every path writes all of its neighbour slots, kept or not
    if (next_level_.size() < level_size * NEIGHBOUR_CELLS) {
      next_level_.resize(level_size * NEIGHBOUR_CELLS);
    }
    const PathState *paths = level_.data();
    PathState *next_paths = next_level_.data();
    size_t next_size = 0;

    for (size_t path_index = 0; path_index < level_size; ++path_index) {
      const PathState path = paths[path_index];
      const auto node = static_cast<int32_t>(path.node);
      const uint32_t children = trie.GetChildrenMask(node);
      const uint32_t first_child = trie.GetFirstChildIndex(node);

      if (trie.IsWord(node) && word_has_been_found_[node] != solve_id_) {
        // mark the word as found for this board
        word_has_been_found_[node] = solve_id_;
        found_words.emplace_back(trie.GetWord(node));
      }

      // Unused neighbours whose letter continues the prefix
      const uint32_t keep = ADJACENT_CELLS[path.cell] & INDEX_MASK &
                            ~path.visited & matching_cells(children);

      for (const uint32_t cell : NEIGHBOUR_LIST[path.cell]) {
        next_paths[next_size] = {
            first_child + std::popcount(children & lower_letters[cell]), cell,
            path.visited | (1U << cell)};
        next_size += (keep >> cell) & 1U;
      }
    }

    std::swap(level_, next_level_);
    level_size = next_size;
  }
}
