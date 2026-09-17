#include <gtest/gtest.h>

#include <memory>

#include "skiplist/skiplist.h"

TEST(SkipListTest, TestGenera) {
  lbds::skip_list::SkipList<int> list;
  int ret = 0;
  for (int i = 1; i <= 20; i++) {
    ret = list.insert(i);
  }
  list.erase(20);
  list.erase(5);
  list.erase(1);
  list.display();

  for (int i = -10; i <= 25; i++) {
    if (list.contains(i)) {
      std::cout << "Found " << i << std::endl;
    } else {
      std::cout << "Not Found " << i << std::endl;
    }
  }

  EXPECT_TRUE(true);
}
