#ifndef COMPACT_TRIE_H
#define COMPACT_TRIE_H

// system includes
#include <array>
#include <bit>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// local includes
#include "Dictionary.h"

/// @brief Read-only, cache-friendly copy of a Dictionary for Boggle solving
/// @details Each node is 8 bytes (the Dictionary's nodes are 112 bytes) and a
/// node's children are stored next to each other, so a child is found with a
/// popcount instead of a 26-entry array. Two Boggle rules are folded in when
/// the trie is built, which removes them from the search loop:
///  - A 'Q' edge leads straight to the "QU" node, because the Q die face is
///    always read as "QU". Words with a Q not followed by U are dropped.
///  - Only words at least `min_word_length` letters long are marked as words.
class CompactTrie {
public:
  /// @brief Letter index that never has a child; use it for invalid cells
  static constexpr uint32_t INVALID_LETTER = Dictionary::ALPHABET_SIZE;

  /// @brief Bit in Node::mask marking the end of a valid word
  static constexpr uint32_t END_OF_WORD_BIT = 1U << 31;

  /// @brief Padded stride of each row in the 2-letter lookup table
  static constexpr size_t PAIR_TABLE_STRIDE = 32;

  /// @brief Compact 8-byte trie node exposed for hot-loop pointer hoisting
  struct Node {
    // Bits 0-25: letters with a child. Bit 31: end of a word.
    uint32_t mask = 0;
    // Index of the child for the lowest set letter bit
    uint32_t first_child = 0;
  };

  /// @brief Builds the compact trie from a Dictionary
  /// @param dictionary The source dictionary (copied; later inserts are not
  /// seen)
  /// @param min_word_length Shorter words are not marked as words
  explicit CompactTrie(const Dictionary &dictionary,
                       uint32_t min_word_length = 1);

  /// @brief Gets the index of the root node
  /// @return The root index (always 0)
  static constexpr int32_t GetRootIndex() { return 0; }

  /// @brief Gets the child of a node for a letter
  /// @param node_index A valid node index
  /// @param letter Letter index, 'A' = 0, up to INVALID_LETTER
  /// @return The index of the child node, or -1 if there is none
  int32_t GetNextNodeIndex(int32_t node_index, uint32_t letter) const {
    const Node node = nodes_[node_index];
    const uint32_t letter_bit = 1U << letter;
    if (!(node.mask & letter_bit)) {
      return -1;
    }
    return GetKnownChildIndex(node_index, letter_bit - 1);
  }

  /// @pre The child exists and lower_letters contains exactly the bits below
  /// its letter. Children are stored in ascending letter order.
  int32_t GetKnownChildIndex(int32_t node_index, uint32_t lower_letters) const {
    const Node node = nodes_[node_index];
    return static_cast<int32_t>(node.first_child +
                                std::popcount(node.mask & lower_letters));
  }

  /// @brief Gets the index of the child for the lowest letter of a node
  /// @details The child for a letter is this index plus the number of child
  /// letters below it. Lets a caller read a node once and find several
  /// children without reading the node again.
  /// @param node_index A valid node index
  /// @return Index of the first child (meaningless when there are no children)
  uint32_t GetFirstChildIndex(int32_t node_index) const {
    return nodes_[node_index].first_child;
  }

  /// @brief Checks if a node ends a word of at least `min_word_length` letters
  bool IsWord(int32_t node_index) const {
    return (nodes_[node_index].mask & END_OF_WORD_BIT) != 0;
  }

  /// @brief Checks if any longer word continues from this node
  bool HasChildren(int32_t node_index) const {
    return GetChildrenMask(node_index) != 0;
  }

  /// @brief Gets the outgoing letter bits, excluding the end-of-word flag
  uint32_t GetChildrenMask(int32_t node_index) const {
    return nodes_[node_index].mask & CHILDREN_MASK;
  }

  /// @brief Gets the word that ends at a node
  /// @param node_index A node for which IsWord() is true
  /// @return The word in upper case, "Q" written as "QU"
  std::string_view GetWord(int32_t node_index) const {
    const uint32_t span = word_spans_[node_index];
    return std::string_view(word_text_.data() + (span >> 8), span & 0xFFU);
  }

  /// @brief Gets the total number of nodes
  size_t GetTotalNodes() const { return nodes_.size(); }

  /// @brief Gets the number of nodes marked as words
  size_t GetTotalWords() const { return total_words_; }

  /// @brief Direct read-only pointer to the contiguous 8-byte node array
  const Node *GetNodesData() const { return nodes_.data(); }

  /// @brief Direct read-only pointer to packed word spans ((offset << 8) | len)
  const uint32_t *GetWordSpansData() const { return word_spans_.data(); }

  /// @brief Direct read-only pointer to the flat word text buffer
  const char *GetWordTextData() const { return word_text_.data(); }

  /// @brief Gets the precomputed 2-letter lookup table indexed as [l0][l1]
  /// @details Row and column INVALID_LETTER (26) are all zeros so invalid
  /// board cells resolve to 0 without branching. Value 0 means no valid
  /// depth-2 node (or a dead depth-2 leaf with no word and no children).
  const std::array<uint16_t, PAIR_TABLE_STRIDE> *GetPairNodeTable() const {
    return pair_nodes_.data();
  }

  /// @brief True if any 1-cell (depth-1) node is marked as a word
  bool HasDepth1Words() const { return has_depth1_words_; }

private:
  static constexpr uint32_t CHILDREN_MASK =
      (1U << Dictionary::ALPHABET_SIZE) - 1;

  std::vector<Node> nodes_;
  size_t total_words_ = 0;
  bool has_depth1_words_ = false;

  // Words are only read when found, so they are kept out of the node array.
  // Each span packs (offset << 8) | length into 4 bytes.
  std::string word_text_;
  std::vector<uint32_t> word_spans_;

  // Direct 2-letter node index table: pair_nodes_[l0][l1] gives the depth-2
  // trie node index (always <= 702 in BFS order, fitting in uint16_t), or 0
  // if that 2-letter prefix cannot lead to any word. Each 64-byte row fits in
  // one cache line, and row/col 26 (INVALID_LETTER) are always 0.
  alignas(64) std::array<std::array<uint16_t, PAIR_TABLE_STRIDE>,
                         Dictionary::ALPHABET_SIZE + 1> pair_nodes_{};
};

#endif // COMPACT_TRIE_H
