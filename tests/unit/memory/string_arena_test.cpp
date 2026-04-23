#include <gtest/gtest.h>
#include <string_view>
#include "autocomplete/memory/StringArena.h"

using autocomplete::memory::StringArena;

TEST(StringArenaTest, AppendReturnsHandleRoundTrips) {
  StringArena arena;
  auto h1 = arena.append("hello");
  auto h2 = arena.append("world");

  EXPECT_EQ(arena.view(h1), "hello");
  EXPECT_EQ(arena.view(h2), "world");
  EXPECT_NE(h1.offset, h2.offset);
}

TEST(StringArenaTest, OffsetsAreStableAcrossGrowth) {
  StringArena arena;
  auto h = arena.append("stable");
  for (int i = 0; i < 10000; ++i) arena.append("padpadpadpad");
  EXPECT_EQ(arena.view(h), "stable");
}

TEST(StringArenaTest, HandleLengthFitsInPhraseLengthField) {
  StringArena arena;
  auto h = arena.append(std::string(40000, 'x'));
  EXPECT_EQ(h.length, 40000u);
  EXPECT_EQ(arena.view(h).size(), 40000u);
}
