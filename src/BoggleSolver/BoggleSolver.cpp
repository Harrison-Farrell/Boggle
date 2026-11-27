#include "BoggleSolver.h"

// system includes
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
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
    // Most boards produce under 200 candidate word hits; start with small
    // L1-friendly buffers rather than allocating per-dictionary-size arrays.
    found_word_nodes_.resize(1024);
    level_.resize(256);
    next_level_.resize(256 * NEIGHBOUR_CELLS + 2);
    active_.resize(256);
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

// Precomputed upper 32 bits of a PackedPath for adding `cell`:
//   bits 32..47 = cell index, bits 48..63 = (1U << cell).
// Entry 32 is zero so std::countr_zero(0) == 32 indexes it safely.
constexpr std::array<uint64_t, 33> CELL_PACKED = [] {
  std::array<uint64_t, 33> tbl{};
  for (uint32_t cell = 0; cell < BOARD_SIZE; ++cell) {
    tbl[cell] = (uint64_t{cell} << 32) | (uint64_t{1U << cell} << 48);
  }
  return tbl;
}();

// Pure random-access iterator that decodes packed word spans on dereference,
// letting std::vector<std::string_view>::assign size the buffer once and write
// every view directly without zero-initializing or per-element capacity checks.
struct WordViewIterator {
  using iterator_category = std::random_access_iterator_tag;
  using value_type = std::string_view;
  using difference_type = std::ptrdiff_t;
  using pointer = const std::string_view *;
  using reference = std::string_view;

  const uint32_t *nodes_ptr;
  const uint32_t *spans;
  const char *text;

  std::string_view operator*() const {
    const uint32_t span = spans[*nodes_ptr];
    return std::string_view(text + (span >> 8), span & 0xFFU);
  }
  std::string_view operator[](difference_type n) const {
    const uint32_t span = spans[nodes_ptr[n]];
    return std::string_view(text + (span >> 8), span & 0xFFU);
  }
  WordViewIterator &operator++() {
    ++nodes_ptr;
    return *this;
  }
  WordViewIterator operator++(int) {
    auto copy = *this;
    ++nodes_ptr;
    return copy;
  }
  WordViewIterator &operator--() {
    --nodes_ptr;
    return *this;
  }
  WordViewIterator &operator+=(difference_type n) {
    nodes_ptr += n;
    return *this;
  }
  WordViewIterator operator+(difference_type n) const {
    return {nodes_ptr + n, spans, text};
  }
  difference_type operator-(const WordViewIterator &other) const {
    return nodes_ptr - other.nodes_ptr;
  }
  bool operator==(const WordViewIterator &other) const {
    return nodes_ptr == other.nodes_ptr;
  }
  bool operator!=(const WordViewIterator &other) const {
    return nodes_ptr != other.nodes_ptr;
  }
  bool operator<(const WordViewIterator &other) const {
    return nodes_ptr < other.nodes_ptr;
  }
};
} // namespace

template <typename Word>
void BoggleSolver::SolveImpl(const std::array<char, BOARD_SIZE> &board,
                             std::vector<Word> &found_words) {
  found_words.clear();
  if (!trie_) [[unlikely]] {
    return;
  }
  const CompactTrie &trie = *trie_;
  const CompactTrie::Node *__restrict nodes = trie.GetNodesData();

  // For each cell, the letters alphabetically before its letter. Counting the
  // children among them gives the cell's child in a node. Entry 32 is zero so
  // std::countr_zero(0) == 32 can index it safely when a slot is unused.
  alignas(32) uint32_t lower_letters[33];
  alignas(16) uint8_t clamped_letters[BOARD_SIZE];

#if defined(__SSSE3__)
  static_assert(BOARD_SIZE == 16, "SIMD letter filtering requires 16 cells");

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
  const __m128i bit_lut =
      _mm_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128);
  const __m128i bit_indices =
      _mm_or_si128(_mm_and_si128(letters_vec, _mm_set1_epi8(7)),
                   _mm_and_si128(invalid_msb, _mm_set1_epi8(-128)));
  const __m128i bit_selectors = _mm_shuffle_epi8(bit_lut, bit_indices);
  const __m128i bias_7f = _mm_set1_epi8(127);

  // Clamp any non-letter byte (>= 26) to INVALID_LETTER (26) for the 2-letter
  // lookup table and lower-letter masks.
  const __m128i clamped = _mm_min_epu8(
      letters_vec,
      _mm_set1_epi8(static_cast<char>(CompactTrie::INVALID_LETTER)));
  _mm_store_si128(reinterpret_cast<__m128i *>(clamped_letters), clamped);
  for (uint32_t cell = 0; cell < BOARD_SIZE; ++cell) {
    lower_letters[cell] = (1U << clamped_letters[cell]) - 1U;
  }
  lower_letters[32] = 0;

  // Project all outgoing trie letters onto the board's 16 cells at once.
  // Each byte of matching_bits is either 0 or a single power-of-two bit
  // (1..128); adding 127 sets bit 7 iff that byte is non-zero, so movemask
  // directly yields the positive 16-bit cell mask without cmpeq or invert.
  auto matching_cells = [&](uint32_t raw_mask) -> uint32_t {
    const __m128i mask_bytes = _mm_set1_epi32(static_cast<int32_t>(raw_mask));
    const __m128i matching_bits = _mm_and_si128(
        _mm_shuffle_epi8(mask_bytes, byte_selectors), bit_selectors);
    return static_cast<uint32_t>(
        _mm_movemask_epi8(_mm_add_epi8(matching_bits, bias_7f)));
  };
#else
  std::array<uint16_t, Dictionary::ALPHABET_SIZE> letter_cells{};
  uint32_t board_letter_mask = 0;
  for (uint32_t cell = 0; cell < BOARD_SIZE; ++cell) {
    const uint32_t letter =
        (static_cast<unsigned char>(board[cell]) & ~0x20U) - 'A';
      if (letter < Dictionary::ALPHABET_SIZE) {
        clamped_letters[cell] = static_cast<uint8_t>(letter);
        lower_letters[cell] = (1U << letter) - 1U;
        letter_cells[letter] |= static_cast<uint16_t>(1U << cell);
        board_letter_mask |= (1U << letter);
      } else {
        clamped_letters[cell] =
            static_cast<uint8_t>(CompactTrie::INVALID_LETTER);
        lower_letters[cell] = 0;
      }
    }
  lower_letters[32] = 0;

  auto matching_cells = [&](uint32_t raw_mask) -> uint32_t {
    uint32_t cells = 0;
    for (uint32_t present = raw_mask & board_letter_mask; present != 0;
         present &= present - 1) {
      cells |= letter_cells[static_cast<uint32_t>(std::countr_zero(present))];
    }
    return cells;
  };
#endif

    // Seed the BFS queue directly at depth 2 using the precomputed 1,728-byte
    // 2-letter lookup table, bypassing Level 0 and Level 1 completely (no
    // depth-1 SIMD filtering, no Level-1 queue writes/reads, and zero keep>=3
    // branches). Fall back to depth 1 only if a custom trie marks 1-cell words.
    if (level_.size() < 85) [[unlikely]] {
      level_.resize(256);
    }
    size_t level_size = 0;
      if (!trie.HasDepth1Words()) [[likely]] {
        PackedPath *__restrict lvl_ptr = level_.data();
        const auto *__restrict pair_tbl = trie.GetPairNodeTable();
        for (uint32_t c0 = 0; c0 < BOARD_SIZE; ++c0) {
          const uint16_t *__restrict row = pair_tbl[clamped_letters[c0]].data();
          uint32_t nbrs = NEIGHBOUR_MASKS[c0];
          const uint64_t v0_hi = uint64_t{1U << c0} << 48;
          do {
            const auto c1 = static_cast<uint32_t>(std::countr_zero(nbrs));
            nbrs &= nbrs - 1;
            const uint32_t n2 = row[clamped_letters[c1]];
            *lvl_ptr = uint64_t{n2} | v0_hi | CELL_PACKED[c1];
            lvl_ptr += (n2 != 0);
          } while (nbrs != 0);
        }
        level_size = static_cast<size_t>(lvl_ptr - level_.data());
      } else {
        const uint32_t root_mask = nodes[CompactTrie::GetRootIndex()].mask;
        const uint32_t root_first_child =
            nodes[CompactTrie::GetRootIndex()].first_child;
        PackedPath *__restrict lvl_ptr = level_.data();
        for (uint32_t starts = matching_cells(root_mask); starts != 0;
             starts &= starts - 1) {
          const auto cell = static_cast<uint32_t>(std::countr_zero(starts));
          const uint32_t child =
              root_first_child + std::popcount(root_mask & lower_letters[cell]);
          lvl_ptr[level_size++] = uint64_t{child} | CELL_PACKED[cell];
        }
      }

      uint32_t *__restrict found_ptr = found_word_nodes_.data();

      while (level_size > 0) {
          if (next_level_.size() < level_size * NEIGHBOUR_CELLS + 2)
              [[unlikely]] {
            next_level_.resize(level_size * NEIGHBOUR_CELLS + 2);
          }
          if (active_.size() <= level_size) [[unlikely]] {
            active_.resize(level_size + 1);
          }
          const auto found_so_far =
              static_cast<size_t>(found_ptr - found_word_nodes_.data());
          if (found_so_far + level_size >= found_word_nodes_.size())
              [[unlikely]] {
            found_word_nodes_.resize(std::max(found_word_nodes_.size() * 2,
                                              found_so_far + level_size + 1));
            found_ptr = found_word_nodes_.data() + found_so_far;
          }

          // Phase A (Filter): load each trie node once, record candidate word
          // hits branchlessly without touching found_word_bits_ (deferred
          // deduplication), compute matching unused neighbours, and drop dead
          // ends (~54% of paths) before any tzcnt, popcnt, or child PathState
          // packing runs.
          const PackedPath *__restrict path_ptr = level_.data();
          const PackedPath *const paths_end = path_ptr + level_size;
          ActiveState *__restrict act_ptr = active_.data();

          for (; path_ptr != paths_end; ++path_ptr) {
            const PackedPath path = *path_ptr;
            const uint32_t node = static_cast<uint32_t>(path);
            const uint32_t cell = static_cast<uint32_t>((path >> 32) & 0xFFFFU);
            const uint32_t visited = static_cast<uint32_t>(path >> 48);

            const CompactTrie::Node nd = nodes[node];
            const uint32_t mask = nd.mask;

            // Record any word hit unconditionally; duplicates are filtered
            // after BFS so the inner loop never touches the 53 KB
            // found_word_bits_ array.
            *found_ptr = node;
            found_ptr += (mask >> 31);

            const uint32_t keep =
                NEIGHBOUR_MASKS[cell] & ~visited & matching_cells(mask);
            *act_ptr = {mask, nd.first_child, visited, keep};
            act_ptr += (keep != 0);
          }

          // Phase B (Expand): multi-pass branchless 2-slot compaction over
          // surviving active paths. Every active entry has keep != 0, emits 1
          // or 2 packed 64-bit PathStates, and compacts entries with remaining
          // bits in place. This eliminates all data-dependent branch
          // mispredictions across levels.
          PackedPath *__restrict next_ptr = next_level_.data();
          ActiveState *__restrict act_base = active_.data();

            while (act_ptr != act_base) {
              ActiveState *__restrict write_act = act_base;
              for (const ActiveState *__restrict it = act_base; it != act_ptr;
                   ++it) {
                const ActiveState st = *it;
                const auto c0 =
                    static_cast<uint32_t>(std::countr_zero(st.keep));
                const uint32_t k1 = st.keep & (st.keep - 1);
                const uint32_t ch0 =
                    st.first_child + std::popcount(st.mask & lower_letters[c0]);

                const auto c1 = static_cast<uint32_t>(std::countr_zero(k1));
                const uint32_t k2 = k1 & (k1 - 1);
                const uint32_t ch1 =
                    st.first_child + std::popcount(st.mask & lower_letters[c1]);

                const uint64_t v_hi = uint64_t{st.visited} << 48;
                next_ptr[0] = uint64_t{ch0} | v_hi | CELL_PACKED[c0];
                next_ptr[1] = uint64_t{ch1} | v_hi | CELL_PACKED[c1];
                next_ptr += 1 + (k1 != 0);

                *write_act = {st.mask, st.first_child, st.visited, k2};
                write_act += (k2 != 0);
              }
              act_ptr = write_act;
            }

            level_size = static_cast<size_t>(next_ptr - next_level_.data());
            std::swap(level_, next_level_);
          }

          // Deduplicate the candidate word hits after the search, preserving
          // first-seen order, then emit word text and clear only the touched
          // 64-bit bitset blocks.
          uint32_t *__restrict found_nodes = found_word_nodes_.data();
          const auto cand_count = static_cast<size_t>(found_ptr - found_nodes);
          if (cand_count == 0) [[unlikely]] {
            return;
          }

          uint64_t *__restrict found_bits = found_word_bits_.data();
            size_t unique_count = 0;
            for (size_t i = 0; i < cand_count; ++i) {
              const uint32_t node = found_nodes[i];
              uint64_t &bits = found_bits[node >> 6];
              const uint64_t node_bit = uint64_t{1} << (node & 63);
              const uint64_t is_new = (bits & node_bit) == 0;
              bits |= node_bit;
              found_nodes[unique_count] = node;
              unique_count += is_new;
            }

            const uint32_t *__restrict spans = trie.GetWordSpansData();
            const char *__restrict text = trie.GetWordTextData();

            if constexpr (std::is_same_v<Word, std::string_view>) {
              found_words.assign(
                  WordViewIterator{found_nodes, spans, text},
                  WordViewIterator{found_nodes + unique_count, spans, text});
              for (size_t i = 0; i < unique_count; ++i) {
                found_bits[found_nodes[i] >> 6] = 0;
              }
            } else {
              found_words.reserve(unique_count);
              for (size_t i = 0; i < unique_count; ++i) {
                const uint32_t node = found_nodes[i];
                const uint32_t span = spans[node];
                found_words.emplace_back(text + (span >> 8), span & 0xFFU);
                found_bits[node >> 6] = 0;
              }
            }
          }
