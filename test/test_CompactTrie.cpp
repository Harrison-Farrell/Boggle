// system includes
#include <bit>
#include <cstdint>
#include <string>
#include <string_view>

// external includes
#include <gtest/gtest.h>

// local includes
#include "Dictionary/CompactTrie.h"
#include "Dictionary/Dictionary.h"

namespace {
/// @brief Follows a word through the trie, reading "QU" from a single 'Q'
int32_t Walk(const CompactTrie &trie, std::string_view letters) {
  int32_t node = CompactTrie::GetRootIndex();
  for (const char letter : letters) {
    node = trie.GetNextNodeIndex(node, static_cast<uint32_t>(letter - 'A'));
    if (node == -1) {
      return -1;
    }
  }
  return node;
}
} // namespace

TEST(CompactTrieTest, FindsInsertedWords) {
  Dictionary dictionary;
  dictionary.Insert("CAR");
  dictionary.Insert("CARD");
  dictionary.Insert("cat");
  const CompactTrie trie(dictionary);

  for (const std::string_view word : {"CAR", "CARD", "CAT"}) {
    const int32_t node = Walk(trie, word);
    ASSERT_NE(node, -1) << word;
    EXPECT_TRUE(trie.IsWord(node)) << word;
    EXPECT_EQ(trie.GetWord(node), word);
  }

  EXPECT_FALSE(trie.IsWord(Walk(trie, "CA")));
  EXPECT_EQ(Walk(trie, "COT"), -1);
}

TEST(CompactTrieTest, HasChildrenOnlyForPrefixes) {
  Dictionary dictionary;
  dictionary.Insert("CAR");
  dictionary.Insert("CARD");
  const CompactTrie trie(dictionary);

  EXPECT_TRUE(trie.HasChildren(Walk(trie, "CAR")));
  EXPECT_FALSE(trie.HasChildren(Walk(trie, "CARD")));
}

TEST(CompactTrieTest, ChildrenMaskExcludesWordFlagAndUsesQuEdge) {
  Dictionary dictionary;
  dictionary.Insert("CAR");
  dictionary.Insert("CARD");
  dictionary.Insert("CARZ");
  dictionary.Insert("QUA");
  const CompactTrie trie(dictionary, 3);

  EXPECT_EQ(trie.GetChildrenMask(Walk(trie, "CAR")),
            (1U << ('D' - 'A')) | (1U << ('Z' - 'A')));
  EXPECT_EQ(trie.GetChildrenMask(Walk(trie, "CARD")), 0U);
  EXPECT_EQ(trie.GetChildrenMask(Walk(trie, "Q")), 1U << ('A' - 'A'));
}

TEST(CompactTrieTest, ShortWordsAreNotMarked) {
  Dictionary dictionary;
  dictionary.Insert("AT");
  dictionary.Insert("ATE");
  const CompactTrie trie(dictionary, 3);

  const int32_t at = Walk(trie, "AT");
  ASSERT_NE(at, -1);
  EXPECT_FALSE(trie.IsWord(at));
  EXPECT_TRUE(trie.IsWord(Walk(trie, "ATE")));
}

TEST(CompactTrieTest, QLeadsStraightToQu) {
  Dictionary dictionary;
  dictionary.Insert("QUIT");
  dictionary.Insert("QAT"); // Q without U can never be made on a board
  const CompactTrie trie(dictionary, 3);

  // "QIT" on the board spells "QUIT"
  const int32_t quit = Walk(trie, "QIT");
  ASSERT_NE(quit, -1);
  EXPECT_TRUE(trie.IsWord(quit));
  EXPECT_EQ(trie.GetWord(quit), "QUIT");

  EXPECT_EQ(Walk(trie, "QAT"), -1);
}

TEST(CompactTrieTest, QuCountsAsTwoLetters) {
  Dictionary dictionary;
  dictionary.Insert("QUA");
  const CompactTrie trie(dictionary, 3);

  // Two cells on the board, but three letters long
  EXPECT_TRUE(trie.IsWord(Walk(trie, "QA")));
}

TEST(CompactTrieTest, InvalidLetterHasNoChild) {
  Dictionary dictionary;
  dictionary.Insert("CAT");
  const CompactTrie trie(dictionary);

  EXPECT_EQ(trie.GetNextNodeIndex(CompactTrie::GetRootIndex(),
                                  CompactTrie::INVALID_LETTER),
            -1);
}

TEST(CompactTrieTest, EmptyDictionary) {
  const Dictionary dictionary;
  const CompactTrie trie(dictionary);

  EXPECT_EQ(trie.GetTotalNodes(), 1U);
  EXPECT_EQ(trie.GetTotalWords(), 0U);
  EXPECT_FALSE(trie.HasChildren(CompactTrie::GetRootIndex()));
}

TEST(CompactTrieTest, FirstChildPlusLowerLettersFindsEachChild) {
  Dictionary dictionary;
  dictionary.Insert("CAB");
  dictionary.Insert("CAN");
  dictionary.Insert("CAT");
  const CompactTrie trie(dictionary);

  const int32_t ca = Walk(trie, "CA");
  ASSERT_NE(ca, -1);
  const uint32_t children = trie.GetChildrenMask(ca);
  const uint32_t first = trie.GetFirstChildIndex(ca);
  for (const char letter : {'B', 'N', 'T'}) {
    const uint32_t lower = (1U << static_cast<uint32_t>(letter - 'A')) - 1;
    EXPECT_EQ(static_cast<int32_t>(first + std::popcount(children & lower)),
              trie.GetNextNodeIndex(ca, static_cast<uint32_t>(letter - 'A')))
        << letter;
  }
}

TEST(CompactTrieTest, CountsOnlyMarkedWords) {
  Dictionary dictionary;
  dictionary.Insert("AT");   // too short
  dictionary.Insert("ATE");
  dictionary.Insert("QAT");  // Q without U is dropped
  dictionary.Insert("QUIT");
  const CompactTrie trie(dictionary, 3);

  EXPECT_EQ(trie.GetTotalWords(), 2U);
}
