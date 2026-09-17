#pragma once

#include <skiplist/skiplist.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "define.h"
#include "util.h"

namespace lbds::skip_list {
using internal::Level;
using internal::MAX_LEVEL;
using internal::Util;

template <typename T>
class Node {
 public:
  explicit Node(const T& inValue, Level inLevel = 0)
      : value(inValue),
        level(inLevel),
        next(static_cast<std::size_t>(inLevel) + 1, nullptr) {}

  explicit Node(T&& inValue, Level inLevel = 0)
      : value(std::move(inValue)),
        level(inLevel),
        next(static_cast<std::size_t>(inLevel) + 1, nullptr) {}

  T value;
  Level level;
  std::vector<std::shared_ptr<Node>> next;
};
template <typename T>
using NodePtr = std::shared_ptr<Node<T>>;

template <typename T, typename Compare>
class SkipList<T, Compare>::Impl {
 public:
  explicit Impl(Compare inCompare) : compare(std::move(inCompare)) {
    head = std::make_shared<Node<T>>(NULL, MAX_LEVEL - 1);

    curLevel = 0;
    totalSize = 0;
    rng = static_cast<uint32_t>(std::time(nullptr)) ^
          static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(this));
    rng = rng == 0 ? 1 : rng;
  }
  Impl() : Impl(Compare()) {}

  // Deep copy.
  Impl(const Impl& other) = delete;
  Impl& operator=(const Impl&) = delete;

  ~Impl() { clear(); }

  [[nodiscard]] bool insert(const T& value);
  [[nodiscard]] bool insert(T&& value);
  [[nodiscard]] bool contains(const T& value) const;
  [[nodiscard]] bool erase(const T& value);

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;

  void display() const;
  void clear() noexcept;

  Compare compare;
  std::size_t totalSize;
  uint32_t rng;

  NodePtr<T> head;
  Level curLevel;

 private:
  template <typename U>
  [[nodiscard]] bool insertValue(U&& value);
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
bool SkipList<T, Compare>::contains(const T& value) const {
  return impl_->contains(value);
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

// private implementation
template <typename T, typename Compare>
template <typename U>
bool SkipList<T, Compare>::Impl::insertValue(U&& value) {
  assert(head != nullptr);

  NodePtr<T> cur = head;
  auto updateNode = std::vector<NodePtr<T>>(MAX_LEVEL, nullptr);

  // Find the inserting position
  for (int i = curLevel; i >= 0; i--) {
    while (cur->next[i] && compare(cur->next[i]->value, value)) {
      cur = cur->next[i];
    }
    updateNode[i] = cur;
  }

  const NodePtr<T>& candidate = cur->next[0];
  // candidate is already exist
  if (candidate && !compare(value, candidate->value)) return false;

  const Level rlevel = Util::randomLevel(rng);

  if (rlevel > curLevel) {
    for (int level = curLevel + 1; level <= rlevel; level++) {
      updateNode[level] = head;
    }
  }

  auto newNode = std::make_shared<Node<T>>(std::forward<U>(value), rlevel);
  if (rlevel > curLevel) curLevel = rlevel;

  for (int i = 0; i <= rlevel; i++) {
    newNode->next[i] = updateNode[i]->next[i];
    updateNode[i]->next[i] = newNode;
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
bool SkipList<T, Compare>::Impl::contains(const T& value) const {
  NodePtr<T> cur = head;
  for (int level = curLevel; level >= 0; --level) {
    while (cur->next[level] && compare(cur->next[level]->value, value)) {
      cur = cur->next[level];
    }
  }

  NodePtr<T> candidate = cur->next[0];
  return candidate && !compare(value, candidate->value);
}

template <typename T, typename Compare>
bool SkipList<T, Compare>::Impl::erase(const T& value) {
  auto cur = head;
  auto updateNode = std::vector<NodePtr<T>>(MAX_LEVEL, nullptr);

  // Find the inserting position
  for (int i = curLevel; i >= 0; i--) {
    while (cur->next[i] && compare(cur->next[i]->value, value)) {
      cur = cur->next[i];
    }
    updateNode[i] = cur;
  }

  // cur->next[0] is the first node >= value. Confirm it's an exact match.
  auto target = cur->next[0];
  if (!target || compare(value, target->value)) return false;

  const Level top = target->level < curLevel ? target->level : curLevel;
  for (Level level = 0; level <= top; ++level) {
    if (updateNode[level]->next[level] != target) break;
    updateNode[level]->next[level] = target->next[level];
  }

  while (curLevel > 0 && !head->next[curLevel]) {
    --curLevel;
  }

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
  for (int level = curLevel; level >= 0; level--) {
    std::cout << "Level " << level << ": ";
    auto cur = head->next[level];
    while (cur) {
      std::cout << cur->value << " - ";
      cur = cur->next[level];
    }
    std::cout << std::endl;
  }
}

template <typename T, typename Compare>
void SkipList<T, Compare>::Impl::clear() noexcept {
  NodePtr<T> node = head->next.empty() ? nullptr : head->next[0];
  for (auto& link : head->next) {
    link.reset();
  }
  while (node) {
    NodePtr<T> nextNode = node->next.empty() ? nullptr : node->next[0];
    for (auto& link : node->next) {
      link.reset();
    }
    node = std::move(nextNode);
  }

  curLevel = 0;
  totalSize = 0;
}

}  // namespace lbds::skip_list
