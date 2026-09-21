#pragma once

#include <skiplist/skiplist.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <iterator>
#include <new>
#include <type_traits>
#include <utility>

#include "define.h"
#include "util.h"

namespace lbds::skip_list {
using internal::Level;
using internal::MAX_LEVEL;
using internal::Util;

template <typename T>
class Node {
 public:
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  [[nodiscard]] Node*& link(Level atLevel) noexcept {
    return *(next_ + atLevel);
  }
  [[nodiscard]] Node* link(Level atLevel) const noexcept {
    return *(next_ + atLevel);
  }

  template <typename U>
  [[nodiscard]] static Node* create(U&& value, Level level) {
    void* raw = allocate(level);
    try {
      return new (raw) Node(std::forward<U>(value), level);
    } catch (...) {
      deallocate(raw, level);
      throw;
    }
  }

  static void destroy(Node* node) noexcept {
    const Level level = node->level;
    node->~Node();
    deallocate(node, level);
  }

  T value;
  Level level;

 private:
  template <typename U>
  Node(U&& inValue, Level inLevel)
      : value(std::forward<U>(inValue)), level(inLevel) {
    for (Level i = 0; i <= inLevel; ++i) link(i) = nullptr;
  }

  ~Node() = default;

  static std::size_t bytesFor(Level atLevel) noexcept {
    return sizeof(Node) + sizeof(Node*) * atLevel;
  }

  // Almost every T is ordinarily aligned, and the plain operator new is the
  // faster path; the aligned overload is only worth reaching for when T
  // actually demands it.
  static constexpr bool overAligned() noexcept {
    return alignof(Node) > __STDCPP_DEFAULT_NEW_ALIGNMENT__;
  }

  static void* allocate(Level atLevel) {
    if constexpr (overAligned()) {
      return ::operator new(bytesFor(atLevel), std::align_val_t{alignof(Node)});
    } else {
      return ::operator new(bytesFor(atLevel));
    }
  }

  static void deallocate(void* raw, Level atLevel) noexcept {
    if constexpr (overAligned()) {
      ::operator delete(raw, bytesFor(atLevel),
                        std::align_val_t{alignof(Node)});
    } else {
      ::operator delete(raw, bytesFor(atLevel));
    }
  }

  Node* next_[1];
};

template <typename T>
using UpdatePath = std::array<Node<T>*, MAX_LEVEL>;

namespace internal {

template <typename T>
class ConstIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = const T*;
  using reference = const T&;

  ConstIterator() noexcept : node_(nullptr) {}
  explicit ConstIterator(const Node<T>* node) noexcept : node_(node) {}

  reference operator*() const noexcept { return node_->value; }
  pointer operator->() const noexcept { return &node_->value; }

  ConstIterator& operator++() noexcept {
    node_ = node_->link(0);
    return *this;
  }

  ConstIterator operator++(int) noexcept {
    ConstIterator previous = *this;
    ++*this;
    return previous;
  }

  friend bool operator==(const ConstIterator& a,
                         const ConstIterator& b) noexcept {
    return a.node_ == b.node_;
  }
  friend bool operator!=(const ConstIterator& a,
                         const ConstIterator& b) noexcept {
    return a.node_ != b.node_;
  }

 private:
  const Node<T>* node_;
};

}  // namespace internal

template <typename T, typename Compare>
class SkipList<T, Compare>::Impl {
  static_assert(std::is_default_constructible_v<T>,
                "SkipList<T> requires T to be default-constructible: the head "
                "sentinel stores a value-initialised T.");

 public:
  explicit Impl(Compare inCompare) : compare(std::move(inCompare)) {
    head = Node<T>::create(T{}, MAX_LEVEL - 1);

    curLevel = 0;
    totalSize = 0;
    rng = static_cast<uint32_t>(std::time(nullptr)) ^
          static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(this));
    rng = rng == 0 ? 1 : rng;
  }
  Impl() : Impl(Compare()) {}

  Impl(const Impl& other) = delete;
  Impl& operator=(const Impl&) = delete;

  ~Impl() {
    clear();
    Node<T>::destroy(head);
  }

  [[nodiscard]] bool insert(const T& value);
  [[nodiscard]] bool insert(T&& value);
  [[nodiscard]] bool erase(const T& value);

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;

  void display() const;
  void clear() noexcept;

  [[nodiscard]] Node<T>* lowerBound(const T& value) const;
  [[nodiscard]] Node<T>* upperBound(const T& value) const;

  Compare compare;
  std::size_t totalSize;
  uint32_t rng;

  Node<T>* head;
  Level curLevel;

 private:
  template <typename U>
  [[nodiscard]] bool insertValue(U&& value);

  // Fills `path` with the last node ordered before `value` at each level and
  // returns the candidate at level 0 -- the shared skeleton of insert and
  // erase.
  Node<T>* descend(const T& value, UpdatePath<T>& path) const;
};

template <typename T, typename Compare>
SkipList<T, Compare>::SkipList() {
  impl_ = std::make_unique<Impl>();
}

template <typename T, typename Compare>
SkipList<T, Compare>::SkipList(Compare compare) {
  impl_ = std::make_unique<Impl>(std::move(compare));
}

template <typename T, typename Compare>
SkipList<T, Compare>::~SkipList() = default;

// Public SkipList wrappers
template <typename T, typename Compare>
bool SkipList<T, Compare>::insert(const T& value) {
  return impl_->insert(value);
}
template <typename T, typename Compare>
bool SkipList<T, Compare>::insert(T&& value) {
  return impl_->insert(std::move(value));
}
template <typename T, typename Compare>
bool SkipList<T, Compare>::erase(const T& value) {
  return impl_->erase(value);
}
template <typename T, typename Compare>
bool SkipList<T, Compare>::empty() const noexcept {
  return impl_->empty();
}
template <typename T, typename Compare>
std::size_t SkipList<T, Compare>::size() const noexcept {
  return impl_->size();
}
template <typename T, typename Compare>
void SkipList<T, Compare>::clear() noexcept {
  impl_->clear();
}
template <typename T, typename Compare>
void SkipList<T, Compare>::display() const {
  impl_->display();
}

template <typename T, typename Compare>
typename SkipList<T, Compare>::const_iterator SkipList<T, Compare>::begin()
    const noexcept {
  return const_iterator(impl_->head->link(0));
}
template <typename T, typename Compare>
typename SkipList<T, Compare>::const_iterator SkipList<T, Compare>::end()
    const noexcept {
  return const_iterator(nullptr);
}
template <typename T, typename Compare>
typename SkipList<T, Compare>::const_iterator SkipList<T, Compare>::cbegin()
    const noexcept {
  return begin();
}
template <typename T, typename Compare>
typename SkipList<T, Compare>::const_iterator SkipList<T, Compare>::cend()
    const noexcept {
  return end();
}
template <typename T, typename Compare>
typename SkipList<T, Compare>::const_iterator SkipList<T, Compare>::find(
    const T& value) const {
  Node<T>* candidate = impl_->lowerBound(value);
  const bool equivalent = candidate && !impl_->compare(value, candidate->value);
  return const_iterator(equivalent ? candidate : nullptr);
}
template <typename T, typename Compare>
typename SkipList<T, Compare>::const_iterator SkipList<T, Compare>::lower_bound(
    const T& value) const {
  return const_iterator(impl_->lowerBound(value));
}
template <typename T, typename Compare>
typename SkipList<T, Compare>::const_iterator SkipList<T, Compare>::upper_bound(
    const T& value) const {
  return const_iterator(impl_->upperBound(value));
}

// private implementation
template <typename T, typename Compare>
Node<T>* SkipList<T, Compare>::Impl::descend(const T& value,
                                             UpdatePath<T>& path) const {
  assert(head != nullptr);

  Node<T>* cur = head;
  // Level is unsigned, so a descending loop cannot test `level >= 0` -- that
  // is always true and the decrement past 0 wraps. Start one above the top and
  // decrement in the condition, which visits curLevel down to 0 and stops.
  for (Level level = curLevel + 1; level-- > 0;) {
    while (cur->link(level) && compare(cur->link(level)->value, value)) {
      cur = cur->link(level);
    }
    path[level] = cur;
  }
  return cur->link(0);
}

template <typename T, typename Compare>
Node<T>* SkipList<T, Compare>::Impl::lowerBound(const T& value) const {
  Node<T>* cur = head;
  for (Level level = curLevel + 1; level-- > 0;) {
    // Move left (same level) if the next Node value is still less than source
    // value
    while (cur->link(level) && compare(cur->link(level)->value, value)) {
      cur = cur->link(level);
    }
  }
  return cur->link(0);
}

template <typename T, typename Compare>
Node<T>* SkipList<T, Compare>::Impl::upperBound(const T& value) const {
  Node<T>* cur = head;
  for (Level level = curLevel + 1; level-- > 0;) {
    // Move left (same level) if the source value is greater or equal next Node
    // value
    while (cur->link(level) && !compare(value, cur->link(level)->value)) {
      cur = cur->link(level);
    }
  }
  return cur->link(0);
}

template <typename T, typename Compare>
template <typename U>
bool SkipList<T, Compare>::Impl::insertValue(U&& value) {
  UpdatePath<T> updateNode{};
  Node<T>* candidate = descend(value, updateNode);

  // descend() stops at the first node not ordered before `value`; it is a
  // duplicate exactly when `value` is not ordered before it either.
  if (candidate && !compare(value, candidate->value)) return false;

  const Level rlevel = Util::randomLevel(rng);

  if (rlevel > curLevel) {
    for (Level level = curLevel + 1; level <= rlevel; level++) {
      updateNode[level] = head;
    }
  }

  Node<T>* newNode = Node<T>::create(std::forward<U>(value), rlevel);
  if (rlevel > curLevel) curLevel = rlevel;

  for (Level level = 0; level <= rlevel; level++) {
    Node<T>* predecessor = updateNode[level];
    newNode->link(level) = predecessor->link(level);
    predecessor->link(level) = newNode;
  }
  totalSize += 1;

  return true;
}

template <typename T, typename Compare>
bool SkipList<T, Compare>::Impl::insert(const T& value) {
  return insertValue(value);
}
template <typename T, typename Compare>
bool SkipList<T, Compare>::Impl::insert(T&& value) {
  return insertValue(std::move(value));
}

template <typename T, typename Compare>
bool SkipList<T, Compare>::Impl::erase(const T& value) {
  UpdatePath<T> updateNode{};
  Node<T>* target = descend(value, updateNode);

  if (!target || compare(value, target->value)) return false;

  const Level top = target->level < curLevel ? target->level : curLevel;
  for (Level level = 0; level <= top; ++level) {
    Node<T>* predecessor = updateNode[level];
    if (predecessor->link(level) != target) break;
    predecessor->link(level) = target->link(level);
  }

  while (curLevel > 0 && !head->link(curLevel)) {
    --curLevel;
  }

  Node<T>::destroy(target);
  totalSize -= 1;
  return true;
}

template <typename T, typename Compare>
bool SkipList<T, Compare>::Impl::empty() const noexcept {
  return totalSize == 0;
}

template <typename T, typename Compare>
std::size_t SkipList<T, Compare>::Impl::size() const noexcept {
  return totalSize;
}

template <typename T, typename Compare>
void SkipList<T, Compare>::Impl::display() const {
  for (Level level = curLevel + 1; level-- > 0;) {
    // Level is a character type, so it needs widening or the stream would
    // emit the raw byte rather than the number.
    std::cout << "Level " << static_cast<unsigned>(level) << ": ";
    const Node<T>* cur = head->link(level);
    while (cur) {
      std::cout << cur->value << " - ";
      cur = cur->link(level);
    }
    std::cout << std::endl;
  }
}

template <typename T, typename Compare>
void SkipList<T, Compare>::Impl::clear() noexcept {
  Node<T>* node = head->link(0);
  while (node != nullptr) {
    Node<T>* nextNode = node->link(0);
    Node<T>::destroy(node);
    node = nextNode;
  }

  for (Level level = 0; level < MAX_LEVEL; ++level) head->link(level) = nullptr;
  curLevel = 0;
  totalSize = 0;
}

}  // namespace lbds::skip_list
