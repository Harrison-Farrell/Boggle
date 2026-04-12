#include "Dictionary.h"

// system includes
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

Dictionary::Dictionary() { arena_.emplace_back(); }

bool Dictionary::Insert(const std::string &word) {
  int32_t current_node_index = 0;

  for (const char single_character : word) {
    // Exit early if the character is not an alphabet letter
    if (!static_cast<bool>(
            std::isalpha(static_cast<unsigned char>(single_character)))) {
      return false;
    }

    const int char_index =
        std::toupper(static_cast<unsigned char>(single_character)) - 'A';

    if (arena_[current_node_index].children[char_index] == -1) {
      // Add a new 'node' to the vector
      const auto new_node_index = static_cast<int32_t>(arena_.size());
      arena_.emplace_back();

      // Add the possible path to the mask
      arena_[current_node_index].children_mask |= (1 << char_index);
      arena_[current_node_index].children[char_index] = new_node_index;
    }
    current_node_index = arena_[current_node_index].children[char_index];
  }
  // Set as end of word
  arena_[current_node_index].is_end_of_word = true;
  return true;
}

bool Dictionary::Search(const std::string &word) const {
  int32_t current_node_index = GetRootIndex();

  for (const char single_character : word) {
    current_node_index = GetNextNodeIndex(current_node_index, single_character);
    if (current_node_index == -1) {
      return false;
    }
  }
  return IsWord(current_node_index);
}

bool Dictionary::StartsWith(const std::string &prefix) const {
  int32_t current_node_index = GetRootIndex();

  for (const char single_character : prefix) {
    current_node_index = GetNextNodeIndex(current_node_index, single_character);
    if (current_node_index == -1) {
      return false;
    }
  }
  return true;
}

std::shared_ptr<Dictionary> ReadWordsFromFile(const std::string &filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Error: Could not open file " << filename << "\n";
    return nullptr;
  }

  std::shared_ptr<Dictionary> root = std::make_shared<Dictionary>();
  std::string word;

  // Read words separated by whitespace
  while (file >> word) {
    if (!root->Insert(word)) {
      std::cerr << "Error: Failed to insert word " << word
                << " into the trie tree.\n";
      return nullptr;
    }
  }

  file.close();
  return root;
}