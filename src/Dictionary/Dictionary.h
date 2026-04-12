#ifndef DICTIONARY_H
#define DICTIONARY_H

// system includes
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/// @brief Represents a trie data structure
/// @details Used to store words from a text file to allow for searching
class Dictionary {
public:
  /// @brief Default constructor for Dictionary
  Dictionary();

  /// @brief Destructor for Dictionary
  ~Dictionary() = default;

  // Delete copy constructor and copy assignment operator
  Dictionary(const Dictionary &) = delete;
  Dictionary &operator=(const Dictionary &) = delete;

  // Delete move constructor and move assignment operator
  Dictionary(Dictionary &&) = delete;
  Dictionary &operator=(Dictionary &&) = delete;

  /// @brief Inserts a word into the trie
  /// @param word word to insert
  /// @return True if the word was inserted successfully, false otherwise
  bool Insert(const std::string &word);

  /// @brief Searches for a word in the trie
  /// @param word word to search for
  /// @return True if the word is found, false otherwise
  bool Search(const std::string &word) const;

  /// @brief Checks if there is a word that starts with the given prefix
  /// @param prefix prefix to search
  /// @return True if a word exists with this prefix, false otherwise
  bool StartsWith(const std::string &prefix) const;

  /// @brief Gets the index of the root node
  /// @return The root index (always 0)
  static constexpr int32_t GetRootIndex() { return 0; }

  /// @brief Gets the index of the next node for a given character
  /// @param current_node_index The index of the current node
  /// @param next_char The character to move to
  /// @return The index of the next node, or -1 if not found
  int32_t GetNextNodeIndex(int32_t current_node_index, char next_char) const {
    const int char_index = static_cast<unsigned char>(next_char) - 'A';
    return GetNextNodeIndex(current_node_index,
                            static_cast<uint8_t>(char_index));
  }

  /// @brief Gets the index of the next node for a given character
  /// @param current_node_index The index of the current node
  /// @param next_char The character to move to
  /// @return The index of the next node, or -1 if not found
  int32_t GetNextNodeIndex(int32_t current_node_index,
                           uint8_t char_index) const {
    if (current_node_index < 0 ||
        current_node_index >= static_cast<int32_t>(arena_.size()))
        [[unlikely]] {
      return -1;
    }
    if (char_index >= ALPHABET_SIZE) [[unlikely]] {
      return -1;
    }

    // Use children_mask to reduce array access
    if (!(arena_[current_node_index].children_mask & (1 << char_index))) {
      return -1;
    }

    return arena_[current_node_index].children[char_index];
  }

  /// @brief Gets the total number of nodes in the trie
  /// @return The size of the arena
  size_t GetTotalNodes() const { return arena_.size(); }

  /// @brief Checks if a node marks the end of a valid word
  /// @param index The index of the node to check
  /// @return True if the node is the end of a word, false otherwise
  bool IsWord(int32_t index) const {
    if (index < 0 || index >= static_cast<int32_t>(arena_.size()))
        [[unlikely]] {
      return false;
    }
    return arena_[index].is_end_of_word;
  }

  // Only handling upper case english letters
  constexpr static const int ALPHABET_SIZE = 26;

private:
  // Trie node structure
  struct Node {
    // Mask to reduce array access
    uint32_t children_mask = 0;
    bool is_end_of_word = false;
    std::array<int32_t, ALPHABET_SIZE> children;
    // Set all children nodes as -1 (non-existant path)
    Node() { children.fill(-1); }
  };

  // Vector to hold all nodes contiguously
  std::vector<Node> arena_;
};

/// @brief Reads words from a file and inserts them into data structure
/// @param filename The name of the file to read from
/// @return A shared pointer to the Dictionary
std::shared_ptr<Dictionary> ReadWordsFromFile(const std::string &filename);

#endif // DICTIONARY_H