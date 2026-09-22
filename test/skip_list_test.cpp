#include <gtest/gtest.h>
#include <skiplist/skiplist.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
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

TEST(SkipListEmpty, FindsNothing) {
  SkipList<int> list;
  EXPECT_EQ(list.find(0), list.end());
  EXPECT_EQ(list.find(-1), list.end());
  EXPECT_EQ(list.find(42), list.end());
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
  // reported success without inserting, and lookup bailed out on `!value`.
  SkipList<int> list;

  EXPECT_TRUE(list.insert(0));
  EXPECT_EQ(list.size(), 1u);
  EXPECT_FALSE(list.empty());
  EXPECT_NE(list.find(0), list.end());

  EXPECT_FALSE(list.insert(0));
  EXPECT_EQ(list.size(), 1u);

  EXPECT_TRUE(list.erase(0));
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(list.find(0), list.end());
}

TEST(SkipListSentinel, NegativeValuesSortBeforeZero) {
  SkipList<int> list;
  for (int value : {0, -5, 3, -1}) EXPECT_TRUE(list.insert(value));
  EXPECT_EQ(Contents(list), Strings({-5, -1, 0, 3}));
  EXPECT_NE(list.find(-5), list.end());
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
// upsert
//
// upsert differs from insert on exactly one case: when an equivalent element
// is already present, insert leaves it alone and upsert assigns over it. Under
// a comparator that only looks at part of the value, "equivalent" and "equal"
// are not the same thing, so Entry carries a payload the ordering cannot see
// -- that payload is what makes a replacement observable at all.
// ---------------------------------------------------------------------------

struct Entry {
  int key = 0;
  std::string payload;
};

struct ByKey {
  bool operator()(const Entry& a, const Entry& b) const {
    return a.key < b.key;
  }
};

// So the display()-based helpers can read an Entry list too.
std::ostream& operator<<(std::ostream& os, const Entry& entry) {
  return os << entry.key << ":" << entry.payload;
}

TEST(SkipListUpsert, AddsValuesThatAreNotYetPresent) {
  SkipList<int> list;
  EXPECT_TRUE(list.upsert(2));
  EXPECT_TRUE(list.upsert(1));
  EXPECT_TRUE(list.upsert(3));
  EXPECT_EQ(list.size(), 3u);
  EXPECT_EQ(Contents(list), Strings({1, 2, 3}));
}

TEST(SkipListUpsert, ReportsFalseAndAddsNothingWhenTheValueIsAlreadyPresent) {
  SkipList<int> list;
  EXPECT_TRUE(list.upsert(7));
  EXPECT_FALSE(list.upsert(7));
  EXPECT_FALSE(list.upsert(7));
  EXPECT_EQ(list.size(), 1u);
  EXPECT_EQ(Contents(list), Strings({7}));
}

TEST(SkipListUpsert, ReplacesTheElementAnInsertWouldHaveRejected) {
  SkipList<Entry, ByKey> list;
  ASSERT_TRUE(list.upsert(Entry{2, "first"}));
  ASSERT_FALSE(list.upsert(Entry{2, "second"}));

  EXPECT_EQ(list.size(), 1u);
  ASSERT_NE(list.find(Entry{2, {}}), list.end());
  EXPECT_EQ(list.find(Entry{2, {}})->payload, "second");

  // The same call through insert leaves the stored element alone, which is the
  // whole of the difference between the two.
  ASSERT_FALSE(list.insert(Entry{2, "third"}));
  EXPECT_EQ(list.find(Entry{2, {}})->payload, "second");
}

TEST(SkipListUpsert, ReplacesTheLargestElement) {
  // The largest element has no successor, so an implementation that reaches
  // through the matched node's level-0 link walks off the end of the list.
  SkipList<Entry, ByKey> list;
  for (int key : {1, 2, 3}) ASSERT_TRUE(list.upsert(Entry{key, "old"}));

  ASSERT_FALSE(list.upsert(Entry{3, "new"}));
  EXPECT_EQ(list.size(), 3u);
  EXPECT_EQ(list.find(Entry{3, {}})->payload, "new");

  // The same case again with nothing else in the list at all.
  SkipList<Entry, ByKey> single;
  ASSERT_TRUE(single.upsert(Entry{1, "old"}));
  ASSERT_FALSE(single.upsert(Entry{1, "new"}));
  EXPECT_EQ(single.find(Entry{1, {}})->payload, "new");
}

TEST(SkipListUpsert, TouchesNoElementButTheMatchedOne) {
  // A replacement belongs in exactly one node. Writing through the search path
  // at every level instead reaches nodes that are not the match -- above the
  // matched node's own height the path points past it -- which either corrupts
  // an unrelated element or runs off the end. With 1000 elements the list
  // spans several levels and most nodes sit at level 0, so a per-level write
  // would be near certain to land somewhere it does not belong.
  constexpr int kCount = 1000;
  SkipList<Entry, ByKey> list;
  for (int key = 0; key < kCount; ++key) {
    ASSERT_TRUE(list.insert(Entry{key, "old"}));
  }
  ASSERT_GT(LevelCount(list), 1) << "the list never grew past level 0";

  for (int key = 0; key < kCount; ++key) {
    ASSERT_FALSE(list.upsert(Entry{key, "new"})) << key;
    ASSERT_EQ(list.size(), static_cast<std::size_t>(kCount)) << key;
  }

  int expectedKey = 0;
  for (const Entry& entry : list) {
    ASSERT_EQ(entry.key, expectedKey) << "an element moved or was overwritten";
    ASSERT_EQ(entry.payload, "new")
        << "key " << entry.key << " was not updated";
    ++expectedKey;
  }
  EXPECT_EQ(expectedKey, kCount);
}

TEST(SkipListUpsert, GrowsTheListPastItsCurrentHeight) {
  // Adding through upsert has to extend the search path above the levels
  // descend() filled in, exactly as insert does.
  SkipList<int> list;
  for (int value = 0; value < 5000; ++value) ASSERT_TRUE(list.upsert(value));

  EXPECT_EQ(list.size(), 5000u);
  EXPECT_GT(LevelCount(list), 1) << "no level above 0 was ever built";
  EXPECT_TRUE(std::is_sorted(list.begin(), list.end()));
}

// ---------------------------------------------------------------------------
// erase
// ---------------------------------------------------------------------------

TEST(SkipListErase, RemovesOnlyTheRequestedValue) {
  SkipList<int> list;
  for (int value : {1, 2, 3, 4, 5}) ASSERT_TRUE(list.insert(value));

  EXPECT_TRUE(list.erase(3));
  EXPECT_EQ(list.size(), 4u);
  EXPECT_EQ(list.find(3), list.end());
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
  EXPECT_EQ(list.find(2), list.end());
  EXPECT_TRUE(list.insert(2));
  EXPECT_NE(list.find(2), list.end());
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
  for (int i = 0; i < 100; ++i) EXPECT_EQ(list.find(i), list.end()) << i;

  EXPECT_TRUE(list.insert(5));
  EXPECT_NE(list.find(5), list.end());
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
  EXPECT_NE(list.find(9), list.end());
  EXPECT_NE(list.find(1), list.end());
  EXPECT_EQ(list.find(4), list.end());
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
  EXPECT_NE(list.find(-3), list.end());
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
  EXPECT_NE(list.find("fig"), list.end());
  EXPECT_EQ(list.find("kiwi"), list.end());
  EXPECT_TRUE(list.erase("apple"));
  EXPECT_EQ(list.size(), 2u);
}

TEST(SkipListTypes, TheEmptyStringIsAnOrdinaryValue) {
  // The string analogue of inserting 0: it equals the sentinel's own value.
  SkipList<std::string> list;
  EXPECT_TRUE(list.insert(std::string{}));
  EXPECT_EQ(list.size(), 1u);
  EXPECT_NE(list.find(std::string{}), list.end());
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
  EXPECT_NE(list.find(Point{0, 9}), list.end());
  EXPECT_EQ(list.find(Point{0, 1}), list.end());
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

TEST(SkipListValueCategory, RvalueUpsertMovesIntoTheNewNode) {
  SkipList<Tracked, TrackedLess> list;
  Tracked::ResetCounters();

  Tracked value{42};
  EXPECT_TRUE(list.upsert(std::move(value)));
  EXPECT_EQ(Tracked::copies, 0);
  EXPECT_EQ(Tracked::moves, 1);
}

TEST(SkipListValueCategory, RvalueUpsertMoveAssignsOverAnExistingElement) {
  SkipList<Tracked, TrackedLess> list;
  ASSERT_TRUE(list.upsert(Tracked{42}));
  Tracked::ResetCounters();

  Tracked replacement{42};
  EXPECT_FALSE(list.upsert(std::move(replacement)));
  EXPECT_EQ(Tracked::copies, 0);
  EXPECT_EQ(Tracked::moves, 1);
  EXPECT_EQ(replacement.value, Tracked::kMovedFrom)
      << "the replacement was copied into place, not moved";
}

TEST(SkipListValueCategory, LvalueUpsertCopyAssignsOverAnExistingElement) {
  SkipList<Tracked, TrackedLess> list;
  ASSERT_TRUE(list.upsert(Tracked{42}));
  Tracked::ResetCounters();

  Tracked replacement{42};
  EXPECT_FALSE(list.upsert(replacement));
  EXPECT_EQ(Tracked::copies, 1);
  EXPECT_EQ(Tracked::moves, 0);
  EXPECT_EQ(replacement.value, 42)
      << "a copying upsert must not consume its source";
}
// ---------------------------------------------------------------------------
// Element lifetime
//
// A leak checker only proves the memory came back, not that ~T() ran -- and
// the short strings elsewhere in this file fit in the small-string buffer, so
// they never allocate and could not expose a skipped destructor either.
// Counting live objects directly is the only thing that does.
// ---------------------------------------------------------------------------

struct Counted {
  static inline int alive = 0;

  int value = 0;

  Counted() { ++alive; }
  explicit Counted(int inValue) : value(inValue) { ++alive; }
  Counted(const Counted& other) : value(other.value) { ++alive; }
  Counted(Counted&& other) noexcept : value(other.value) { ++alive; }
  Counted& operator=(const Counted&) = default;
  Counted& operator=(Counted&&) = default;
  ~Counted() { --alive; }
};

struct CountedLess {
  bool operator()(const Counted& a, const Counted& b) const {
    return a.value < b.value;
  }
};

TEST(SkipListLifetime, ClearAndDestructionDestroyEveryElement) {
  const int baseline = Counted::alive;
  {
    SkipList<Counted, CountedLess> list;
    for (int value = 0; value < 100; ++value) {
      ASSERT_TRUE(list.insert(Counted{value}));
    }
    EXPECT_EQ(Counted::alive, baseline + 101)
        << "100 elements plus the head sentinel";

    list.clear();
    EXPECT_EQ(Counted::alive, baseline + 1)
        << "clear() must destroy every element, leaving only the sentinel";
  }
  EXPECT_EQ(Counted::alive, baseline)
      << "the list's destructor must destroy the sentinel too";
}

TEST(SkipListLifetime, EraseDestroysTheElement) {
  const int baseline = Counted::alive;
  SkipList<Counted, CountedLess> list;

  ASSERT_TRUE(list.insert(Counted{1}));
  EXPECT_EQ(Counted::alive, baseline + 2);

  ASSERT_TRUE(list.erase(Counted{1}));
  EXPECT_EQ(Counted::alive, baseline + 1) << "erase() must destroy the element";
}

TEST(SkipListLifetime, RejectedInsertDestroysNothingExtra) {
  const int baseline = Counted::alive;
  SkipList<Counted, CountedLess> list;
  ASSERT_TRUE(list.insert(Counted{1}));

  const int afterFirst = Counted::alive;
  EXPECT_FALSE(list.insert(Counted{1}));
  EXPECT_EQ(Counted::alive, afterFirst) << "a rejected insert built no node";
  EXPECT_EQ(afterFirst, baseline + 2);
}

TEST(SkipListLifetime, ReplacingAnElementBuildsNoExtraNode) {
  const int baseline = Counted::alive;
  SkipList<Counted, CountedLess> list;
  ASSERT_TRUE(list.insert(Counted{1}));

  const int afterFirst = Counted::alive;
  EXPECT_FALSE(list.upsert(Counted{1}));
  EXPECT_EQ(Counted::alive, afterFirst)
      << "a replacement assigns over the element in place";
  EXPECT_EQ(afterFirst, baseline + 2);
}

TEST(SkipListLifetime, ReleasesHeapOwnedByElements) {
  // Long strings allocate, so a skipped destructor here shows up under a leak
  // checker as well as in the counter above.
  SkipList<std::string> list;
  for (int i = 0; i < 200; ++i) {
    ASSERT_TRUE(list.insert(std::string(128, static_cast<char>('a' + i % 26)) +
                            std::to_string(i)));
  }
  EXPECT_EQ(list.size(), 200u);
  list.clear();
  EXPECT_TRUE(list.empty());
}

// ---------------------------------------------------------------------------
// Iteration and ordered queries
// ---------------------------------------------------------------------------

TEST(SkipListIteration, EmptyListIteratesOverNothing) {
  SkipList<int> list;
  EXPECT_EQ(list.begin(), list.end());
  EXPECT_EQ(std::distance(list.begin(), list.end()), 0);
}

TEST(SkipListIteration, VisitsEveryValueInOrder) {
  SkipList<int> list;
  for (int value : {5, 1, 9, 3, 7}) ASSERT_TRUE(list.insert(value));

  const std::vector<int> visited(list.begin(), list.end());
  EXPECT_EQ(visited, (std::vector<int>{1, 3, 5, 7, 9}));
  EXPECT_EQ(visited.size(), list.size());
  EXPECT_TRUE(std::is_sorted(list.begin(), list.end()));
}

TEST(SkipListIteration, AgreesWithDisplay) {
  // display() was the only observation channel before iterators existed; the
  // two must not be able to disagree.
  SkipList<int> list;
  for (int value : {4, 2, 8, 6}) ASSERT_TRUE(list.insert(value));

  std::vector<std::string> iterated;
  for (int value : list) iterated.push_back(std::to_string(value));
  EXPECT_EQ(iterated, Contents(list));
}

TEST(SkipListIteration, FollowsTheComparator) {
  SkipList<int, std::greater<int>> list;
  for (int value : {1, 5, 3}) ASSERT_TRUE(list.insert(value));

  EXPECT_EQ((std::vector<int>(list.begin(), list.end())),
            (std::vector<int>{5, 3, 1}));
}

TEST(SkipListIteration, SurvivesErasureOfOtherElements) {
  SkipList<int> list;
  for (int value = 0; value < 10; ++value) ASSERT_TRUE(list.insert(value));
  for (int value = 0; value < 10; value += 2) ASSERT_TRUE(list.erase(value));

  EXPECT_EQ((std::vector<int>(list.begin(), list.end())),
            (std::vector<int>{1, 3, 5, 7, 9}));
}

TEST(SkipListIteration, PostIncrementReturnsThePreviousPosition) {
  SkipList<int> list;
  ASSERT_TRUE(list.insert(1));
  ASSERT_TRUE(list.insert(2));

  auto it = list.begin();
  const auto before = it++;
  EXPECT_EQ(*before, 1);
  EXPECT_EQ(*it, 2);
}

TEST(SkipListIteration, WorksWithNonTrivialValues) {
  SkipList<std::string> list;
  for (const char* value : {"pear", "apple", "fig"}) {
    ASSERT_TRUE(list.insert(std::string{value}));
  }

  auto it = list.begin();
  EXPECT_EQ(*it, "apple");
  EXPECT_EQ(it->size(), 5u);  // operator-> reaches through to the value
}

// ---------------------------------------------------------------------------
// find -- the one lookup API, so membership is find() != end()
// ---------------------------------------------------------------------------

TEST(SkipListFind, DistinguishesPresentFromAbsent) {
  SkipList<int> list;
  for (int value : {10, 20, 30, 40}) ASSERT_TRUE(list.insert(value));

  for (int value : {10, 20, 30, 40}) {
    EXPECT_NE(list.find(value), list.end()) << value;
  }
  for (int value : {9, 11, 25, 39, 41, -10}) {
    EXPECT_EQ(list.find(value), list.end()) << value;
  }
}

TEST(SkipListFind, LeavesTheListUnchanged) {
  SkipList<int> list;
  for (int value : {1, 2, 3}) ASSERT_TRUE(list.insert(value));

  EXPECT_NE(list.find(2), list.end());
  EXPECT_EQ(list.find(99), list.end());
  EXPECT_EQ(list.size(), 3u);
  EXPECT_EQ(Contents(list), Strings({1, 2, 3}));
}

TEST(SkipListFind, LocatesPresentValuesAndReportsAbsentOnes) {
  SkipList<int> list;
  for (int value : {10, 20, 30}) ASSERT_TRUE(list.insert(value));

  const auto found = list.find(20);
  ASSERT_NE(found, list.end());
  EXPECT_EQ(*found, 20);

  EXPECT_EQ(list.find(25), list.end());
  EXPECT_EQ(list.find(0), list.end());
  EXPECT_EQ(list.find(99), list.end());
}

TEST(SkipListFind, OnAnEmptyListReturnsEnd) {
  SkipList<int> list;
  EXPECT_EQ(list.find(1), list.end());
}

TEST(SkipListBounds, LowerBoundIncludesAnExactMatch) {
  SkipList<int> list;
  for (int value : {10, 20, 30}) ASSERT_TRUE(list.insert(value));

  EXPECT_EQ(*list.lower_bound(20), 20);
  EXPECT_EQ(*list.lower_bound(15), 20) << "rounds up to the next value";
  EXPECT_EQ(*list.lower_bound(0), 10);
  EXPECT_EQ(list.lower_bound(31), list.end());
}

TEST(SkipListBounds, UpperBoundSkipsAnExactMatch) {
  SkipList<int> list;
  for (int value : {10, 20, 30}) ASSERT_TRUE(list.insert(value));

  EXPECT_EQ(*list.upper_bound(20), 30);
  EXPECT_EQ(*list.upper_bound(15), 20);
  EXPECT_EQ(list.upper_bound(30), list.end());
}

TEST(SkipListBounds, BracketAHalfOpenRange) {
  SkipList<int> list;
  for (int value = 0; value < 100; value += 10) ASSERT_TRUE(list.insert(value));

  // Every value in [30, 60], inclusive at both ends.
  const std::vector<int> window(list.lower_bound(30), list.upper_bound(60));
  EXPECT_EQ(window, (std::vector<int>{30, 40, 50, 60}));
}

TEST(SkipListBounds, OnAnEmptyListReturnEnd) {
  SkipList<int> list;
  EXPECT_EQ(list.lower_bound(1), list.end());
  EXPECT_EQ(list.upper_bound(1), list.end());
}

TEST(SkipListBounds, MatchStdSetAcrossEveryProbe) {
  // The bounds are the reason an ordered container exists, so check them
  // exhaustively against the reference rather than at a few hand-picked spots.
  SkipList<int> list;
  std::set<int> reference;
  for (int value = 0; value < 50; value += 3) {
    ASSERT_TRUE(list.insert(value));
    reference.insert(value);
  }

  for (int probe = -2; probe <= 52; ++probe) {
    const auto expectedLower = reference.lower_bound(probe);
    const auto actualLower = list.lower_bound(probe);
    if (expectedLower == reference.end()) {
      EXPECT_EQ(actualLower, list.end()) << "lower_bound(" << probe << ")";
    } else {
      ASSERT_NE(actualLower, list.end()) << "lower_bound(" << probe << ")";
      EXPECT_EQ(*actualLower, *expectedLower) << "lower_bound(" << probe << ")";
    }

    const auto expectedUpper = reference.upper_bound(probe);
    const auto actualUpper = list.upper_bound(probe);
    if (expectedUpper == reference.end()) {
      EXPECT_EQ(actualUpper, list.end()) << "upper_bound(" << probe << ")";
    } else {
      ASSERT_NE(actualUpper, list.end()) << "upper_bound(" << probe << ")";
      EXPECT_EQ(*actualUpper, *expectedUpper) << "upper_bound(" << probe << ")";
    }
  }
}

TEST(SkipListIteration, IsAForwardIteratorTheStandardAlgorithmsAccept) {
  SkipList<int> list;
  for (int value : {3, 1, 2}) ASSERT_TRUE(list.insert(value));

  using Iter = SkipList<int>::const_iterator;
  static_assert(std::is_same_v<std::iterator_traits<Iter>::iterator_category,
                               std::forward_iterator_tag>);
  static_assert(std::is_same_v<std::iterator_traits<Iter>::value_type, int>);

  EXPECT_EQ(std::count(list.begin(), list.end(), 2), 1);
  EXPECT_EQ(std::accumulate(list.begin(), list.end(), 0), 6);
  EXPECT_NE(std::find(list.begin(), list.end(), 3), list.end());
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

  for (int i = 0; i < kCount; ++i) ASSERT_NE(list.find(i), list.end()) << i;
  EXPECT_EQ(list.find(-1), list.end());
  EXPECT_EQ(list.find(kCount), list.end());

  for (int i = 0; i < kCount; i += 2) ASSERT_TRUE(list.erase(i)) << i;
  EXPECT_EQ(list.size(), static_cast<std::size_t>(kCount / 2));
  for (int i = 0; i < kCount; ++i) {
    ASSERT_EQ(list.find(i) != list.end(), i % 2 == 1) << i;
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
        ASSERT_EQ(list.find(value) != list.end(), reference.count(value) != 0)
            << "find(" << value << ") at step " << step;
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

TEST(SkipListStress, UpsertMatchesInsertOrAssignUnderRandomOperations) {
  // std::map::insert_or_assign is upsert's closest standard analogue, down to
  // the meaning of the returned bool, so it makes a ready-made oracle.
  std::mt19937 rng(20260922);
  std::uniform_int_distribution<int> keyDist(0, 99);
  std::uniform_int_distribution<int> opDist(0, 9);

  SkipList<Entry, ByKey> list;
  std::map<int, std::string> reference;

  for (int step = 0; step < 4000; ++step) {
    const int key = keyDist(rng);
    const std::string payload = std::to_string(step);
    const int choice = opDist(rng);

    if (choice < 6) {
      const bool added = list.upsert(Entry{key, payload});
      ASSERT_EQ(added, reference.insert_or_assign(key, payload).second)
          << "upsert(" << key << ") at step " << step;
    } else if (choice < 8) {
      const bool inserted = list.insert(Entry{key, payload});
      ASSERT_EQ(inserted, reference.emplace(key, payload).second)
          << "insert(" << key << ") at step " << step;
    } else {
      const bool erased = list.erase(Entry{key, {}});
      ASSERT_EQ(erased, reference.erase(key) != 0)
          << "erase(" << key << ") at step " << step;
    }

    ASSERT_EQ(list.size(), reference.size()) << "at step " << step;
  }

  // Every surviving element must carry the payload of the most recent write to
  // its key, and the keys must still be in order.
  auto expected = reference.begin();
  for (const Entry& entry : list) {
    ASSERT_NE(expected, reference.end());
    ASSERT_EQ(entry.key, expected->first);
    ASSERT_EQ(entry.payload, expected->second)
        << "stale payload for key " << entry.key;
    ++expected;
  }
  EXPECT_EQ(expected, reference.end());
}

}  // namespace
