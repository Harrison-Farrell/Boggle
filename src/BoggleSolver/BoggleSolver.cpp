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

// local includes
#include "BoggleBoard/BoggleBoard.h"
#include "Dictionary/Dictionary.h"

BoggleSolver::BoggleSolver(std::shared_ptr<Dictionary> dictionary)
    : dictionary_(std::move(dictionary)) {
  if (dictionary_) {
    word_has_been_found_.resize(dictionary_->GetTotalNodes(), 0);
  }

  // the maximum word length, stop allocation during DFS
  current_word_buffer_.reserve(MAX_WORD_LENGTH);
}

std::vector<std::string>
BoggleSolver::Solve(const std::array<char, BOARD_SIZE> &board) {
  std::vector<std::string> found_words;
  if (!dictionary_) [[unlikely]] {
    return found_words;
  }

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

  // Recursive lambda to replace the recursive function call
  auto depth_first_search = [&](this auto &&self, uint32_t current_grid_index,
                                uint32_t visited_mask,
                                int32_t current_node_index) -> void {
    // Mark current cell as visited
    visited_mask |= (1 << current_grid_index);

    const char single_char = board[current_grid_index];
    const size_t original_length = current_word_buffer_.length();

    // The 'Q' to 'QU' rule was handled in the parent when finding the node
    if (single_char == 'Q') {
      current_word_buffer_ += "QU";
    } else {
      current_word_buffer_ += single_char;
    }

    // Check if current_word is a word over 3 letters & has not been found
    if (current_word_buffer_.length() >= 3 &&
        dictionary_->IsWord(current_node_index) &&
        word_has_been_found_[current_node_index] != solve_id_) {
      // mark the word as found for this board
      word_has_been_found_[current_node_index] = solve_id_;
      found_words.push_back(current_word_buffer_);
    }

    // Get the precomputed neighbours of the current cell
    uint32_t bitmask = ADJACENT_CELLS[current_grid_index] & INDEX_MASK;

    while (bitmask > 0) {
      const uint32_t next_index = std::countr_zero(bitmask);

      // Skip if already visited
      if (!(visited_mask & (1 << next_index))) {
        const char next_char = board[next_index];

        // Check if next char leads to a valid node before recursing
        int32_t next_node =
            dictionary_->GetNextNodeIndex(current_node_index, next_char);

        if (next_node != -1) {
          // Special 'Q' rule handling in lookup
          if (next_char == 'Q') {
            next_node = dictionary_->GetNextNodeIndex(next_node, 'U');
          }

          if (next_node != -1) {
            self(next_index, visited_mask, next_node);
          }
        }
      }

      // Clear the lowest set bit
      bitmask &= (bitmask - 1);
    }

    // Restore original word length (backtrack)
    current_word_buffer_.resize(original_length);
  };

  // Start a search from every letter on the board
  for (uint32_t current_grid_index = 0; current_grid_index < BOARD_SIZE;
       ++current_grid_index) {
    const char start_char = board[current_grid_index];

    int32_t current_node_index =
        dictionary_->GetNextNodeIndex(dictionary_->GetRootIndex(), start_char);

    if (current_node_index != -1) {
      // Handle 'QU' before entering the recursive function
      if (start_char == 'Q') {
        current_node_index =
            dictionary_->GetNextNodeIndex(current_node_index, 'U');
      }

      if (current_node_index != -1) {
        current_word_buffer_.clear();
        depth_first_search(current_grid_index, 0, current_node_index);
      }
    }
  }

  return found_words;
}
