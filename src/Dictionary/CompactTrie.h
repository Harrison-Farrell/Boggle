#ifndef COMPACT_TRIE_H
#define COMPACT_TRIE_H

// system includes
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
    return (nodes_[node_index].mask & WORD_BIT) != 0;
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
    const WordSpan span = word_spans_[node_index];
    return std::string_view(word_text_).substr(span.offset, span.length);
  }

  /// @brief Gets the total number of nodes
  size_t GetTotalNodes() const { return nodes_.size(); }

  /// @brief Gets the number of nodes marked as words
  size_t GetTotalWords() const { return total_words_; }

private:
  static constexpr uint32_t WORD_BIT = 1U << 31;
  static constexpr uint32_t CHILDREN_MASK =
      (1U << Dictionary::ALPHABET_SIZE) - 1;

  struct Node {
    // Bits 0-25: letters with a child. Bit 31: end of a word.
    uint32_t mask = 0;
    // Index of the child for the lowest set letter bit
    uint32_t first_child = 0;
  };

  struct WordSpan {
    uint32_t offset = 0;
    uint32_t length = 0;
  };

  std::vector<Node> nodes_;
  size_t total_words_ = 0;

  // Words are only read when found, so they are kept out of the node array
  std::string word_text_;
  std::vector<WordSpan> word_spans_;
};

#endif // COMPACT_TRIE_H
