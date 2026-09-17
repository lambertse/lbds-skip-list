#include <gtest/gtest.h>
#include <skiplist/skiplist.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using lbds::skip_list::SkipList;

// ---------------------------------------------------------------------------
// Helpers
//
// The list exposes no iterator, so display() is the only channel for observing
// ordering and height. These helpers parse it rather than asserting on its
// exact formatting, so a cosmetic change to display() touches one place.
// ---------------------------------------------------------------------------

std::string CaptureDisplay(const std::function<void()>& render) {
  testing::internal::CaptureStdout();
  render();
  return testing::internal::GetCapturedStdout();
}

// The full ordered contents, read off the level-0 chain.
template <typename List>
std::vector<std::string> Contents(const List& list) {
  const std::string captured = CaptureDisplay([&list] { list.display(); });

  // display() prints one line per level, highest first, so the level-0 line --
  // the only chain holding every element -- is the last one.
  std::istringstream lines(captured);
  std::string line;
  std::string chain;
  while (std::getline(lines, line)) {
    if (line.rfind("Level 0: ", 0) == 0) chain = line.substr(9);
  }

  std::vector<std::string> values;
  for (std::size_t start = 0;;) {
    const std::size_t separator = chain.find(" - ", start);
    if (separator == std::string::npos) break;
    values.push_back(chain.substr(start, separator - start));
    start = separator + 3;
  }
  return values;
}

// How many levels the list currently spans (1 means "level 0 only").
template <typename List>
int LevelCount(const List& list) {
  const std::string captured = CaptureDisplay([&list] { list.display(); });
  std::istringstream lines(captured);
  std::string line;
  int levels = 0;
  while (std::getline(lines, line)) {
    if (line.rfind("Level ", 0) == 0) ++levels;
  }
  return levels;
}

std::vector<std::string> Strings(std::initializer_list<int> values) {
  std::vector<std::string> out;
  for (int value : values) out.push_back(std::to_string(value));
  return out;
}

// ---------------------------------------------------------------------------
// Empty list
// ---------------------------------------------------------------------------

TEST(SkipListEmpty, StartsEmpty) {
  SkipList<int> list;
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(list.size(), 0u);
  EXPECT_EQ(LevelCount(list), 1);
}

TEST(SkipListEmpty, ContainsNothing) {
  SkipList<int> list;
  EXPECT_FALSE(list.contains(0));
  EXPECT_FALSE(list.contains(-1));
  EXPECT_FALSE(list.contains(42));
}

TEST(SkipListEmpty, EraseReportsFailure) {
  SkipList<int> list;
  EXPECT_FALSE(list.erase(0));
  EXPECT_FALSE(list.erase(7));
  EXPECT_TRUE(list.empty());
}

TEST(SkipListEmpty, DisplayIsSafe) {
  SkipList<int> list;
  EXPECT_TRUE(Contents(list).empty());
}

// ---------------------------------------------------------------------------
// The head sentinel must never be mistaken for a real value
// ---------------------------------------------------------------------------

TEST(SkipListSentinel, ZeroIsAnOrdinaryValue) {
  // Regression: the sentinel was built from NULL, so insert() matched it and
  // reported success without inserting, and contains() bailed out on `!value`.
  SkipList<int> list;

  EXPECT_TRUE(list.insert(0));
  EXPECT_EQ(list.size(), 1u);
  EXPECT_FALSE(list.empty());
  EXPECT_TRUE(list.contains(0));

  EXPECT_FALSE(list.insert(0));
  EXPECT_EQ(list.size(), 1u);

  EXPECT_TRUE(list.erase(0));
  EXPECT_TRUE(list.empty());
  EXPECT_FALSE(list.contains(0));
}

TEST(SkipListSentinel, NegativeValuesSortBeforeZero) {
  SkipList<int> list;
  for (int value : {0, -5, 3, -1}) EXPECT_TRUE(list.insert(value));
  EXPECT_EQ(Contents(list), Strings({-5, -1, 0, 3}));
  EXPECT_TRUE(list.contains(-5));
  EXPECT_TRUE(list.erase(-5));
  EXPECT_EQ(Contents(list), Strings({-1, 0, 3}));
}

// ---------------------------------------------------------------------------
// insert
// ---------------------------------------------------------------------------

TEST(SkipListInsert, ReportsTrueOnlyWhenTheValueIsNew) {
  SkipList<int> list;
  EXPECT_TRUE(list.insert(7));
  EXPECT_FALSE(list.insert(7));
  EXPECT_FALSE(list.insert(7));
  EXPECT_EQ(list.size(), 1u);
  EXPECT_EQ(Contents(list), Strings({7}));
}

TEST(SkipListInsert, KeepsValuesSortedWhateverTheInsertionOrder) {
  SkipList<int> ascending;
  SkipList<int> descending;
  SkipList<int> shuffled;

  for (int value : {1, 2, 3, 4, 5}) EXPECT_TRUE(ascending.insert(value));
  for (int value : {5, 4, 3, 2, 1}) EXPECT_TRUE(descending.insert(value));
  for (int value : {3, 1, 5, 2, 4}) EXPECT_TRUE(shuffled.insert(value));

  const std::vector<std::string> expected = Strings({1, 2, 3, 4, 5});
  EXPECT_EQ(Contents(ascending), expected);
  EXPECT_EQ(Contents(descending), expected);
  EXPECT_EQ(Contents(shuffled), expected);
}

TEST(SkipListInsert, BuildsExpressLanesForLargeInputs) {
  SkipList<int> list;
  for (int i = 0; i < 5000; ++i) ASSERT_TRUE(list.insert(i));
  // With P = 1/4 the chance that not one of 5000 nodes is promoted is ~0.
  EXPECT_GT(LevelCount(list), 1) << "no level above 0 was ever built";
}

// ---------------------------------------------------------------------------
// contains
// ---------------------------------------------------------------------------

TEST(SkipListContains, DistinguishesPresentFromAbsent) {
  SkipList<int> list;
  for (int value : {10, 20, 30, 40}) ASSERT_TRUE(list.insert(value));

  for (int value : {10, 20, 30, 40}) EXPECT_TRUE(list.contains(value)) << value;
  for (int value : {9, 11, 25, 39, 41, -10}) {
    EXPECT_FALSE(list.contains(value)) << value;
  }
}

TEST(SkipListContains, LeavesTheListUnchanged) {
  SkipList<int> list;
  for (int value : {1, 2, 3}) ASSERT_TRUE(list.insert(value));

  EXPECT_TRUE(list.contains(2));
  EXPECT_FALSE(list.contains(99));
  EXPECT_EQ(list.size(), 3u);
  EXPECT_EQ(Contents(list), Strings({1, 2, 3}));
}

// ---------------------------------------------------------------------------
// erase
// ---------------------------------------------------------------------------

TEST(SkipListErase, RemovesOnlyTheRequestedValue) {
  SkipList<int> list;
  for (int value : {1, 2, 3, 4, 5}) ASSERT_TRUE(list.insert(value));

  EXPECT_TRUE(list.erase(3));
  EXPECT_EQ(list.size(), 4u);
  EXPECT_FALSE(list.contains(3));
  EXPECT_EQ(Contents(list), Strings({1, 2, 4, 5}));
}

TEST(SkipListErase, ReportsFalseForAbsentValues) {
  SkipList<int> list;
  for (int value : {1, 2, 3}) ASSERT_TRUE(list.insert(value));

  EXPECT_FALSE(list.erase(0));
  EXPECT_FALSE(list.erase(4));
  EXPECT_EQ(list.size(), 3u);
  EXPECT_EQ(Contents(list), Strings({1, 2, 3}));
}

TEST(SkipListErase, HandlesTheFirstAndLastElements) {
  SkipList<int> list;
  for (int value : {1, 2, 3}) ASSERT_TRUE(list.insert(value));

  EXPECT_TRUE(list.erase(1));
  EXPECT_EQ(Contents(list), Strings({2, 3}));
  EXPECT_TRUE(list.erase(3));
  EXPECT_EQ(Contents(list), Strings({2}));
  EXPECT_TRUE(list.erase(2));
  EXPECT_TRUE(list.empty());
}

TEST(SkipListErase, AllowsTheValueToBeReinserted) {
  SkipList<int> list;
  for (int value : {1, 2, 3}) ASSERT_TRUE(list.insert(value));

  EXPECT_TRUE(list.erase(2));
  EXPECT_FALSE(list.contains(2));
  EXPECT_TRUE(list.insert(2));
  EXPECT_TRUE(list.contains(2));
  EXPECT_EQ(Contents(list), Strings({1, 2, 3}));
}

TEST(SkipListErase, ShrinksTheHeightWhenTallNodesGo) {
  SkipList<int> list;
  for (int i = 0; i < 2000; ++i) ASSERT_TRUE(list.insert(i));
  ASSERT_GT(LevelCount(list), 1);

  for (int i = 0; i < 2000; ++i) ASSERT_TRUE(list.erase(i)) << i;
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(LevelCount(list), 1) << "the height never shrank back";
}

// ---------------------------------------------------------------------------
// clear
// ---------------------------------------------------------------------------

TEST(SkipListClear, EmptiesTheListAndLeavesItUsable) {
  SkipList<int> list;
  for (int i = 0; i < 100; ++i) ASSERT_TRUE(list.insert(i));

  list.clear();
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(list.size(), 0u);
  EXPECT_EQ(LevelCount(list), 1);
  for (int i = 0; i < 100; ++i) EXPECT_FALSE(list.contains(i)) << i;

  EXPECT_TRUE(list.insert(5));
  EXPECT_TRUE(list.contains(5));
  EXPECT_EQ(list.size(), 1u);
}

TEST(SkipListClear, IsSafeOnAnEmptyListAndIsIdempotent) {
  SkipList<int> list;
  list.clear();
  list.clear();
  EXPECT_TRUE(list.empty());
  EXPECT_TRUE(list.insert(1));
  EXPECT_EQ(list.size(), 1u);
}

// ---------------------------------------------------------------------------
// Comparators
// ---------------------------------------------------------------------------

TEST(SkipListComparator, GreaterOrdersDescending) {
  SkipList<int, std::greater<int>> list;
  for (int value : {5, 1, 9, 3}) ASSERT_TRUE(list.insert(value));

  EXPECT_EQ(Contents(list), Strings({9, 5, 3, 1}));
  EXPECT_TRUE(list.contains(9));
  EXPECT_TRUE(list.contains(1));
  EXPECT_FALSE(list.contains(4));
  EXPECT_TRUE(list.erase(5));
  EXPECT_EQ(Contents(list), Strings({9, 3, 1}));
}

TEST(SkipListComparator, RejectsDuplicatesUnderACustomOrdering) {
  // Regression: duplicate detection compared with operator== rather than the
  // comparator, so it disagreed with the ordering for non-default comparators.
  SkipList<int, std::greater<int>> list;
  EXPECT_TRUE(list.insert(2));
  EXPECT_FALSE(list.insert(2));
  EXPECT_EQ(list.size(), 1u);
}

TEST(SkipListComparator, AcceptsAComparatorInstance) {
  // Regression: this constructor built the unique_ptr from the comparator
  // itself, which did not compile.
  SkipList<int, std::greater<int>> list{std::greater<int>{}};
  EXPECT_TRUE(list.insert(1));
  EXPECT_TRUE(list.insert(2));
  EXPECT_EQ(Contents(list), Strings({2, 1}));
}

struct ByMagnitude {
  bool operator()(int a, int b) const { return std::abs(a) < std::abs(b); }
};

TEST(SkipListComparator, TreatsEquivalentValuesAsDuplicates) {
  // -3 and 3 are equivalent under this ordering even though they differ, which
  // only works if membership is decided by the comparator.
  SkipList<int, ByMagnitude> list;
  EXPECT_TRUE(list.insert(3));
  EXPECT_FALSE(list.insert(-3));
  EXPECT_EQ(list.size(), 1u);
  EXPECT_TRUE(list.contains(-3));
  EXPECT_TRUE(list.erase(-3));
  EXPECT_TRUE(list.empty());
}

// ---------------------------------------------------------------------------
// Element types
// ---------------------------------------------------------------------------

TEST(SkipListTypes, SupportsStdString) {
  // Regression: the sentinel was built from NULL, which for std::string picked
  // string(const char*) with a null pointer -- undefined behaviour on
  // construction, before any operation ran.
  SkipList<std::string> list;
  for (const char* text : {"pear", "apple", "fig"}) {
    ASSERT_TRUE(list.insert(std::string(text)));
  }

  EXPECT_EQ(Contents(list), (std::vector<std::string>{"apple", "fig", "pear"}));
  EXPECT_TRUE(list.contains("fig"));
  EXPECT_FALSE(list.contains("kiwi"));
  EXPECT_TRUE(list.erase("apple"));
  EXPECT_EQ(list.size(), 2u);
}

TEST(SkipListTypes, TheEmptyStringIsAnOrdinaryValue) {
  // The string analogue of inserting 0: it equals the sentinel's own value.
  SkipList<std::string> list;
  EXPECT_TRUE(list.insert(std::string{}));
  EXPECT_EQ(list.size(), 1u);
  EXPECT_TRUE(list.contains(std::string{}));
  EXPECT_FALSE(list.insert(std::string{}));
  EXPECT_TRUE(list.erase(std::string{}));
  EXPECT_TRUE(list.empty());
}

struct Point {
  int x;
  int y;
};

struct PointLess {
  bool operator()(const Point& a, const Point& b) const {
    return a.x != b.x ? a.x < b.x : a.y < b.y;
  }
};

TEST(SkipListTypes, SupportsUserDefinedTypes) {
  // Point is neither convertible to bool nor constructible from a null pointer
  // constant, which both used to be hard requirements. display() is not called
  // here, so no operator<< is needed.
  SkipList<Point, PointLess> list;
  EXPECT_TRUE(list.insert(Point{1, 2}));
  EXPECT_TRUE(list.insert(Point{0, 9}));
  EXPECT_FALSE(list.insert(Point{1, 2}));

  EXPECT_EQ(list.size(), 2u);
  EXPECT_TRUE(list.contains(Point{0, 9}));
  EXPECT_FALSE(list.contains(Point{0, 1}));
  EXPECT_TRUE(list.erase(Point{1, 2}));
  EXPECT_EQ(list.size(), 1u);
}

// ---------------------------------------------------------------------------
// Value category: insert(T&&) must move, insert(const T&) must copy once
// ---------------------------------------------------------------------------

struct Tracked {
  static constexpr int kMovedFrom = -1;

  int value = 0;
  static inline int copies = 0;
  static inline int moves = 0;

  Tracked() = default;
  explicit Tracked(int inValue) : value(inValue) {}
  Tracked(const Tracked& other) : value(other.value) { ++copies; }
  // Marking the source makes "this was copied, not moved" observable.
  Tracked(Tracked&& other) noexcept : value(other.value) {
    other.value = kMovedFrom;
    ++moves;
  }
  Tracked& operator=(const Tracked& other) {
    value = other.value;
    ++copies;
    return *this;
  }
  Tracked& operator=(Tracked&& other) noexcept {
    value = other.value;
    other.value = kMovedFrom;
    ++moves;
    return *this;
  }

  static void ResetCounters() {
    copies = 0;
    moves = 0;
  }
};

struct TrackedLess {
  bool operator()(const Tracked& a, const Tracked& b) const {
    return a.value < b.value;
  }
};

TEST(SkipListValueCategory, RvalueInsertMovesAndNeverCopies) {
  // Regression: insert(T&&) forwarded its named parameter as an lvalue, so the
  // move overload silently copied.
  SkipList<Tracked, TrackedLess> list;
  Tracked::ResetCounters();  // the sentinel's own construction is not under
                             // test

  Tracked value{42};
  EXPECT_TRUE(list.insert(std::move(value)));
  EXPECT_EQ(Tracked::copies, 0);
  EXPECT_EQ(Tracked::moves, 1);
}

TEST(SkipListValueCategory, LvalueInsertCopiesExactlyOnce) {
  SkipList<Tracked, TrackedLess> list;
  Tracked::ResetCounters();

  Tracked value{42};
  EXPECT_TRUE(list.insert(value));
  EXPECT_EQ(Tracked::copies, 1);
  EXPECT_EQ(Tracked::moves, 0);
  EXPECT_EQ(value.value, 42) << "a copying insert must not consume its source";
}

TEST(SkipListValueCategory, RejectedDuplicatesDoNotTouchTheValue) {
  SkipList<Tracked, TrackedLess> list;
  ASSERT_TRUE(list.insert(Tracked{1}));
  Tracked::ResetCounters();

  EXPECT_FALSE(list.insert(Tracked{1}));
  EXPECT_EQ(Tracked::copies, 0);
  EXPECT_EQ(Tracked::moves, 0) << "a rejected insert still built a node";
}

// ---------------------------------------------------------------------------
// Scale and randomised cross-checks
// ---------------------------------------------------------------------------

TEST(SkipListStress, HandlesTenThousandElements) {
  constexpr int kCount = 10000;

  std::vector<int> values(kCount);
  std::iota(values.begin(), values.end(), 0);
  std::mt19937 rng(20240917);
  std::shuffle(values.begin(), values.end(), rng);

  SkipList<int> list;
  for (int value : values) ASSERT_TRUE(list.insert(value)) << value;
  EXPECT_EQ(list.size(), static_cast<std::size_t>(kCount));

  for (int i = 0; i < kCount; ++i) ASSERT_TRUE(list.contains(i)) << i;
  EXPECT_FALSE(list.contains(-1));
  EXPECT_FALSE(list.contains(kCount));

  for (int i = 0; i < kCount; i += 2) ASSERT_TRUE(list.erase(i)) << i;
  EXPECT_EQ(list.size(), static_cast<std::size_t>(kCount / 2));
  for (int i = 0; i < kCount; ++i) {
    ASSERT_EQ(list.contains(i), i % 2 == 1) << i;
  }
}

TEST(SkipListStress, MatchesStdSetUnderRandomOperations) {
  std::mt19937 rng(424242);
  std::uniform_int_distribution<int> valueDist(0, 199);
  std::uniform_int_distribution<int> opDist(0, 9);

  for (int trial = 0; trial < 20; ++trial) {
    SkipList<int> list;
    std::set<int> reference;

    for (int step = 0; step < 2000; ++step) {
      const int value = valueDist(rng);
      const int choice = opDist(rng);

      if (choice < 5) {
        const bool inserted = list.insert(value);
        ASSERT_EQ(inserted, reference.insert(value).second)
            << "insert(" << value << ") at step " << step;
      } else if (choice < 8) {
        const bool erased = list.erase(value);
        ASSERT_EQ(erased, reference.erase(value) != 0)
            << "erase(" << value << ") at step " << step;
      } else {
        ASSERT_EQ(list.contains(value), reference.count(value) != 0)
            << "contains(" << value << ") at step " << step;
      }

      ASSERT_EQ(list.size(), reference.size()) << "at step " << step;
      ASSERT_EQ(list.empty(), reference.empty()) << "at step " << step;
    }

    std::vector<std::string> expected;
    expected.reserve(reference.size());
    for (int value : reference) expected.push_back(std::to_string(value));
    ASSERT_EQ(Contents(list), expected)
        << "contents diverged in trial " << trial;
  }
}

}  // namespace
