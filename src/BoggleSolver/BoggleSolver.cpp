#include "BoggleSolver.h"

// system includes
#include <algorithm>
#include <array>
#include <bit>
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
    found_word_bits_.resize((trie_->GetTotalNodes() + 63) / 64, 0);
    // Most boards find under 200 words; start with a small L1-friendly buffer
    // rather than allocating 555 KB per solver thread up front.
    found_word_nodes_.resize(
        std::min<size_t>(trie_->GetTotalWords() + 1, 1024));
    level_.resize(256);
    next_level_.resize(256 * NEIGHBOUR_CELLS);
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
// Precomputed 16-bit neighbour masks without the upper neighbor-count bits.
constexpr std::array<uint32_t, BOARD_SIZE> NEIGHBOUR_MASKS = [] {
  std::array<uint32_t, BOARD_SIZE> masks{};
  for (uint32_t cell = 0; cell < BOARD_SIZE; ++cell) {
    masks[cell] = BoggleSolver::ADJACENT_CELLS[cell] & INDEX_MASK;
  }
  return masks;
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

  // For each cell, the letters alphabetically before its letter. Counting the
  // children among them gives the cell's child in a node. Entry 32 is zero so
  // std::countr_zero(0) == 32 can index it safely when a slot is unused.
  alignas(32) std::array<uint32_t, 33> lower_letters{};
#if defined(__SSSE3__)
  static_assert(BOARD_SIZE == 16, "SIMD letter filtering requires 16 cells");
  for (uint32_t cell = 0; cell < BOARD_SIZE; ++cell) {
    const uint32_t letter =
        ((static_cast<unsigned char>(board[cell]) & ~0x20U) - 'A') & 31U;
    lower_letters[cell] = (1U << letter) - 1U;
  }

  // Build the 16-byte SIMD shuffle and bit masks directly in XMM registers.
  // ASCII upper and lower case letters differ only in bit 5; clearing it places
  // valid letters in 0..25 after subtracting 'A'. Unsigned saturating add with
  // (128 - 26) sets bit 7 (0x80) if and only if a byte is >= 26, which causes
  // _mm_shuffle_epi8 to zero the bit selector for any non-letter cell.
  const __m128i raw_board =
      _mm_loadu_si128(reinterpret_cast<const __m128i *>(board.data()));
  const __m128i letters_vec = _mm_sub_epi8(
      _mm_and_si128(raw_board, _mm_set1_epi8(static_cast<char>(~0x20))),
      _mm_set1_epi8('A'));
  const __m128i invalid_msb =
      _mm_adds_epu8(letters_vec, _mm_set1_epi8(static_cast<char>(128 - 26)));
  const __m128i byte_selectors =
      _mm_and_si128(_mm_srli_epi16(letters_vec, 3), _mm_set1_epi8(3));
  const __m128i bit_lut = _mm_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4,
                                        8, 16, 32, 64, -128);
  const __m128i bit_indices =
      _mm_or_si128(_mm_and_si128(letters_vec, _mm_set1_epi8(7)),
                   _mm_and_si128(invalid_msb, _mm_set1_epi8(-128)));
  const __m128i bit_selectors = _mm_shuffle_epi8(bit_lut, bit_indices);
#else
  std::array<uint32_t, BOARD_SIZE> letters{};
  for (uint32_t cell = 0; cell < BOARD_SIZE; ++cell) {
    const uint32_t letter =
        (static_cast<unsigned char>(board[cell]) & ~0x20U) - 'A';
    letters[cell] = letter < Dictionary::ALPHABET_SIZE
                        ? letter
                        : CompactTrie::INVALID_LETTER;
    if (letters[cell] != CompactTrie::INVALID_LETTER) {
      lower_letters[cell] = (1U << letters[cell]) - 1;
    }
  }
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

  const int32_t root = CompactTrie::GetRootIndex();
  const uint32_t root_children = trie.GetChildrenMask(root);
  const uint32_t root_first_child = trie.GetFirstChildIndex(root);

  if (level_.size() < BOARD_SIZE) [[unlikely]] {
    level_.resize(BOARD_SIZE);
  }
  size_t level_size = 0;
  for (uint32_t starts = matching_cells(root_children); starts != 0;
       starts &= starts - 1) {
    const auto cell = static_cast<uint32_t>(std::countr_zero(starts));
    level_[level_size++] = {
        root_first_child + std::popcount(root_children & lower_letters[cell]),
        static_cast<uint16_t>(cell), static_cast<uint16_t>(1U << cell)};
  }

  uint32_t *__restrict found_ptr = found_word_nodes_.data();

  while (level_size > 0) {
    if (next_level_.size() < level_size * NEIGHBOUR_CELLS) [[unlikely]] {
      next_level_.resize(level_size * NEIGHBOUR_CELLS);
    }
    const auto found_so_far =
        static_cast<size_t>(found_ptr - found_word_nodes_.data());
    if (found_so_far + level_size >= found_word_nodes_.size()) [[unlikely]] {
      found_word_nodes_.resize(
          std::max(found_word_nodes_.size() * 2, found_so_far + level_size + 1));
      found_ptr = found_word_nodes_.data() + found_so_far;
    }
    const PathState *__restrict path_ptr = level_.data();
    const PathState *const paths_end = path_ptr + level_size;
    PathState *__restrict next_ptr = next_level_.data();
    uint64_t *__restrict found_bits = found_word_bits_.data();

    for (; path_ptr != paths_end; ++path_ptr) {
      const PathState path = *path_ptr;
      const auto node = static_cast<int32_t>(path.node);
      // Bit 31 (END_OF_WORD_BIT) is never matched by board letters (0..25) or
      // included in lower_letters (bits 0..24), so the raw mask works directly.
      const uint32_t children = trie.GetChildrenMask(node);
      const uint32_t first_child = trie.GetFirstChildIndex(node);

      // Record a new word without branching: between one in eight and one in
      // four visited nodes end a word, which the CPU cannot predict. The node
      // is always written, and the pointer only moves on for a new word.
      uint64_t &bits = found_bits[path.node / 64];
      const uint64_t node_bit = uint64_t{1} << (path.node % 64);
      const uint64_t is_new_word =
          static_cast<uint64_t>(trie.IsWord(node)) & ((bits & node_bit) == 0);
      *found_ptr = path.node;
      found_ptr += is_new_word;
      bits |= node_bit & (0 - is_new_word);

      // Unused neighbours whose letter continues the prefix
      const uint32_t initial_keep =
          NEIGHBOUR_MASKS[path.cell] & ~uint32_t{path.visited} &
          matching_cells(children);
      uint32_t keep = initial_keep;

      // Most paths have 0, 1, or 2 matching neighbours. Extract two lowest set
      // bits unconditionally (countr_zero(0) == 32 reads lower_letters[32] == 0)
      // and handle >= 3 matching neighbours in a rarely taken tail.
      const auto cell0 = static_cast<uint32_t>(std::countr_zero(keep));
      keep &= keep - 1;
      next_ptr[0] = {
          first_child + std::popcount(children & lower_letters[cell0]),
          static_cast<uint16_t>(cell0),
          static_cast<uint16_t>(path.visited | (uint64_t{1} << cell0))};

      const auto cell1 = static_cast<uint32_t>(std::countr_zero(keep));
      keep &= keep - 1;
      next_ptr[1] = {
          first_child + std::popcount(children & lower_letters[cell1]),
          static_cast<uint16_t>(cell1),
          static_cast<uint16_t>(path.visited | (uint64_t{1} << cell1))};

      if (keep != 0) [[unlikely]] {
        PathState *out = next_ptr + 2;
        do {
          const auto cell = static_cast<uint32_t>(std::countr_zero(keep));
          keep &= keep - 1;
          *out++ = {
              first_child + std::popcount(children & lower_letters[cell]),
              static_cast<uint16_t>(cell),
              static_cast<uint16_t>(path.visited | (1U << cell))};
        } while (keep != 0);
      }
      next_ptr += std::popcount(initial_keep);
    }

    level_size = static_cast<size_t>(next_ptr - next_level_.data());
    std::swap(level_, next_level_);
  }

  // Look up the text of the found words now rather than during the search.
  // These reads are spread across large arrays, and here they do not depend
  // on each other, so the CPU can wait on several cache misses at once.
  const uint32_t *__restrict found_nodes = found_word_nodes_.data();
  const auto found_count = static_cast<size_t>(found_ptr - found_nodes);
  found_words.reserve(found_count);
  uint64_t *__restrict found_bits = found_word_bits_.data();
  for (size_t index = 0; index < found_count; ++index) {
    const uint32_t node = found_nodes[index];
    found_words.emplace_back(trie.GetWord(static_cast<int32_t>(node)));
    // Every set bit belongs to a found word, so clear its whole 64-bit block
    found_bits[node / 64] = 0;
  }
}
