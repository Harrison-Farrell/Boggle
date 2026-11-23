#include "CompactTrie.h"

// system includes
#include <algorithm>
#include <bit>
#include <cstdint>
#include <string>
#include <vector>

// local includes
#include "Dictionary.h"

namespace {
constexpr uint32_t LETTER_Q = 'Q' - 'A';
constexpr uint32_t LETTER_U = 'U' - 'A';
} // namespace

CompactTrie::CompactTrie(const Dictionary &dictionary,
                         uint32_t min_word_length) {
  // Nodes are laid out breadth first, so the children of each node end up
  // next to each other in the order they are discovered.
  struct Pending {
    int32_t source;  // node in the Dictionary
    uint32_t parent; // index of the parent in the compact trie
    uint32_t letter; // letter on the edge from the parent
    uint32_t depth;  // word length so far ("QU" counts as two)
  };

  std::vector<Pending> queue;
  queue.reserve(dictionary.GetTotalNodes());
  queue.push_back({Dictionary::GetRootIndex(), 0, 0, 0});
  nodes_.reserve(dictionary.GetTotalNodes());

  for (size_t head = 0; head < queue.size(); ++head) {
    // Copy: push_back below may reallocate the queue
    const Pending current = queue[head];

    Node node;
    node.first_child = static_cast<uint32_t>(queue.size());

    uint32_t letters = dictionary.GetChildrenMask(current.source);
    while (letters != 0) {
      const auto letter = static_cast<uint32_t>(std::countr_zero(letters));
      letters &= letters - 1;

      int32_t child = dictionary.GetNextNodeIndex(
          current.source, static_cast<uint8_t>(letter));
      uint32_t depth = current.depth + 1;

      // The Q face is read as "QU": skip straight to the U node
      if (letter == LETTER_Q) {
        child = dictionary.GetNextNodeIndex(child, static_cast<uint8_t>(LETTER_U));
        ++depth;
        if (child == -1) {
          continue;
        }
      }

      node.mask |= 1U << letter;
      queue.push_back({child, static_cast<uint32_t>(head), letter, depth});
    }

    if (dictionary.IsWord(current.source) && current.depth >= min_word_length) {
      node.mask |= WORD_BIT;
    }
    nodes_.push_back(node);
  }

  // Rebuild the text of each word by walking up to the root
  word_spans_.resize(nodes_.size());
  std::string word;
  for (size_t index = 0; index < nodes_.size(); ++index) {
    if (!(nodes_[index].mask & WORD_BIT)) {
      continue;
    }
    word.clear();
    for (size_t node = index; node != 0; node = queue[node].parent) {
      if (queue[node].letter == LETTER_Q) {
        word += "UQ"; // reversed below
      } else {
        word += static_cast<char>('A' + queue[node].letter);
      }
    }
    std::ranges::reverse(word);

    word_spans_[index] = {static_cast<uint32_t>(word_text_.size()),
                          static_cast<uint32_t>(word.size())};
    word_text_ += word;
  }
}
