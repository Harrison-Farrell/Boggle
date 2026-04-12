// system includes
#include <cstdio>
#include <fstream>

// external includes
#include <gtest/gtest.h>

// local includes
#include "Dictionary/Dictionary.h"

namespace {
class DictionaryTest : public ::testing::Test {
protected:
  Dictionary dictionary;
};
} // namespace

TEST_F(DictionaryTest, InitiallyEmpty) {
  EXPECT_FALSE(dictionary.Search("APPLE"));
  EXPECT_FALSE(dictionary.Search("TEST"));
  EXPECT_FALSE(dictionary.Search(""));
}

TEST_F(DictionaryTest, InsertAndSearchWord) {
  EXPECT_TRUE(dictionary.Insert("APPLE"));
  EXPECT_TRUE(dictionary.Search("APPLE"));
}

TEST_F(DictionaryTest, SearchPrefixFails) {
  EXPECT_TRUE(dictionary.Insert("APPLE"));
  EXPECT_FALSE(dictionary.Search("APP"));
}

TEST_F(DictionaryTest, SearchLongerWordFails) {
  EXPECT_TRUE(dictionary.Insert("APPLE"));
  EXPECT_FALSE(dictionary.Search("APPLES"));
}

TEST_F(DictionaryTest, InsertMultipleWords) {
  EXPECT_TRUE(dictionary.Insert("car"));
  EXPECT_TRUE(dictionary.Insert("card"));
  EXPECT_TRUE(dictionary.Insert("cards"));

  EXPECT_TRUE(dictionary.Search("CAR"));
  EXPECT_TRUE(dictionary.Search("CARD"));
  EXPECT_TRUE(dictionary.Search("CARDS"));

  EXPECT_FALSE(dictionary.Search("CA"));
  EXPECT_FALSE(dictionary.Search("CARDING"));
  EXPECT_FALSE(dictionary.Search("CARDBOARD"));
}

TEST_F(DictionaryTest, DuplicateInsertions) {
  EXPECT_TRUE(dictionary.Insert("HELLO"));
  EXPECT_TRUE(dictionary.Insert("HELLO"));
  EXPECT_TRUE(dictionary.Search("HELLO"));
}

TEST_F(DictionaryTest, InsertInvalidCharactersFails) {
  EXPECT_FALSE(dictionary.Insert("APPLE1"));
  EXPECT_FALSE(dictionary.Insert("HELLO_world"));
  EXPECT_FALSE(dictionary.Insert("app-le"));
}

TEST_F(DictionaryTest, ReadWordsFromFileValid) {
  const std::string filename = "test_words.txt";
  std::ofstream out(filename);
  out << "APPLE\nbanana\norange\n";
  out.close();

  auto file_dictionary = ReadWordsFromFile(filename);
  ASSERT_NE(file_dictionary, nullptr);
  EXPECT_TRUE(file_dictionary->Search("APPLE"));
  EXPECT_TRUE(file_dictionary->Search("BANANA"));
  EXPECT_TRUE(file_dictionary->Search("ORANGE"));
  EXPECT_FALSE(file_dictionary->Search("GRAPE"));

  std::remove(filename.c_str());
}

TEST_F(DictionaryTest, ReadWordsFromFileInvalid) {
  auto file_dictionary = ReadWordsFromFile("does_not_exist_file12345.txt");
  EXPECT_EQ(file_dictionary, nullptr);
}

TEST_F(DictionaryTest, ReadWordsFromFileWithInvalidWordsFails) {
  const std::string filename = "test_invalid_words.txt";
  std::ofstream out(filename);
  out << "APPLE\nbanana123\norange\n";
  out.close();

  auto file_dictionary = ReadWordsFromFile(filename);
  EXPECT_EQ(file_dictionary, nullptr);

  std::remove(filename.c_str());
}
