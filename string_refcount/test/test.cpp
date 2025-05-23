#include <gtest/gtest.h>
#include "string_handle.h"
#include <string>

// manual free to disable leak sanitizer warning
void ManualFree(const StringHandle& h1) {
  delete[] h1.get();
}

TEST(CreateTest, Default) {
  StringHandle h;
  EXPECT_EQ(h.get(), nullptr);
  EXPECT_EQ(h.Count(), 2);
}

TEST(CreateTest, String) {
  StringHandle h{"hello world!"};
  EXPECT_STREQ(h.get(), "hello world!");
  EXPECT_EQ(h.Count(), 1);
}

TEST(CreateTest, StringHandle) {
  StringHandle h1{"hello"};
  {
    StringHandle h2{h1};
    EXPECT_STREQ(h2.get(), "hello");
    EXPECT_EQ(h2.Count(), 2);
    EXPECT_EQ(h1.Count(), 2);
  }
  ManualFree(h1);
}

TEST(AssignTest, String) {
  StringHandle h1{"hello"};
  h1 = "world";
  EXPECT_STREQ(h1.get(), "world");
  EXPECT_EQ(h1.Count(), 1);
  EXPECT_EQ(h1.DeallocCount(), 1);
}

TEST(AssignTest, StringHandle) {
  StringHandle h1{"hello"};
  StringHandle h2{"world"};
  h2 = h1;
  EXPECT_STREQ(h2.get(), "hello");
  EXPECT_EQ(h2.Count(), 2);
  EXPECT_EQ(h1.Count(), 2);
  EXPECT_EQ(h2.DeallocCount(), 1);
  ManualFree(h1);
}

TEST(AssignTest, LeftNull) {
  StringHandle h{nullptr};
  h = "foo";
  EXPECT_EQ(h.Count(), 1);
  EXPECT_STREQ(h.get(), "foo");
  EXPECT_EQ(h.DeallocCount(), 0);
}

TEST(AssignTest, RightNull) {
  StringHandle h{"foo"};
  h = nullptr;
  EXPECT_EQ(h.Count(), 2);
  EXPECT_EQ(h.get(), nullptr);
  EXPECT_EQ(h.DeallocCount(), 1);
}

TEST(AssignMoveTest, StringHandle) {
  StringHandle h1{"hello"};
  h1 = StringHandle{"world"};
  EXPECT_STREQ(h1.get(), "world");
  EXPECT_EQ(h1.Count(), 1);
  EXPECT_EQ(h1.DeallocCount(), 1);
}

TEST(SmokeTest, ThreePlusStrings) {
  StringHandle h1{"hello"};
  StringHandle h2{h1};
  StringHandle h3{h2};
  EXPECT_EQ(h1.Count(), 2);
  EXPECT_EQ(h2.Count(), 2);
  EXPECT_EQ(h3.Count(), 2);
  ManualFree(h1);
}

TEST(SmokeTest, HeapAllocatedString) {
  std::string str{"Some long string"};
  StringHandle h{str.c_str()};
  EXPECT_EQ(h.Count(), 1);
  EXPECT_STREQ(h.get(), str.c_str());
}

TEST(SmokeTest, SelfRef) {
  StringHandle h{"foo"};
  h = h;
  EXPECT_EQ(h.Count(), 1);
  EXPECT_EQ(h.DeallocCount(), 0);
}

TEST(SmokeTest, SelfMoveRef) {
  StringHandle h{"foo"};
  h = std::move(h);
  EXPECT_EQ(h.Count(), 1);
  EXPECT_EQ(h.DeallocCount(), 0);
}

TEST(SortTest, Bubble) {
  std::vector<StringHandle> strings;
  strings.emplace_back("baa");
  strings.emplace_back("aab");
  strings.emplace_back("a");
  strings.emplace_back("aba");

  std::vector<const char *> sorted = {
    "a",
    "aab",
    "aba",
    "baa"
  };

  for (size_t i = 1; i < strings.size(); ++i) {
    for (size_t j = i; j > 0; --j) {
      if (strcmp(strings[j].get(), strings[j - 1].get()) < 0) {
        std::swap(strings[j], strings[j - 1]);
      }
    }
  }
  for (size_t i = 0; i < strings.size(); ++i) {
    ASSERT_STREQ(strings[i].get(), sorted[i]);
    ASSERT_EQ(strings[i].Count(), 1);
  }
}
