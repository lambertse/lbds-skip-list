#include <gtest/gtest.h>
#include <memory>
#include "skiplist/skiplist.h"

TEST(SkipListTest, BuildsSuccessfully) {
  std::unique_ptr<lbds::skip_list::SkipList<int>> list;
  // list->insert(2);
  // list->contains(2);
  // int i = 2;
  // list->contains(i);
  // list->erase(2);
  // list->size();
  // list->empty();
  EXPECT_TRUE(true);
}
