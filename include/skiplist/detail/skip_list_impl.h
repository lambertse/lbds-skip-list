#pragma once

#include <skiplist/skiplist.h>

#include <cstddef>
#include <cstdint>
#include <ctime>
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
  using NodePtr = std::shared_ptr<Node>;

 public:
  Node(const T& inValue, Level inLevel = 1, NodePtr inNext = nullptr)
      : value(inValue),
        level(inLevel),
        next(static_cast<std::size_t>(inLevel), inNext) {}

  T value;
  Level level;
  std::vector<NodePtr> next;
};
template <typename T>
using NodePtr = std::shared_ptr<Node<T>>;

template <typename T, typename Compare>
class SkipList<T, Compare>::Impl {
  // The head sentinel is a full Node and therefore holds a T. Splitting a
  // value-less NodeBase out of Node would lift this requirement; until then,
  // fail here with a readable message instead of deep inside make_shared.
  static_assert(std::is_default_constructible_v<T>,
                "SkipList<T> requires T to be default-constructible: the head "
                "sentinel stores a value-initialised T.");

 public:
  explicit Impl(Compare inCompare) : compare(std::move(inCompare)) {
    head = std::make_shared<Node<T>>(T{}, MAX_LEVEL);

    curLevel = 0;
    totalSize = 0;
    rng = static_cast<uint32_t>(std::time(nullptr)) ^
          static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(this));
    rng = rng == 0 ? 1 : rng;
  }

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
  void clear() noexcept;

  Compare compare;
  std::size_t totalSize;
  uint32_t rng;

  NodePtr<T> head;
  Level curLevel;
};

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

// private implementation
template <typename T, typename Compare>
bool SkipList<T, Compare>::Impl::insert(const T& value) {
  return true;
}
template <typename T, typename Compare>
bool SkipList<T, Compare>::Impl::insert(T&& value) {
  return true;
}
template <typename T, typename Compare>
bool SkipList<T, Compare>::Impl::contains(const T& value) const {
  return true;
}

template <typename T, typename Compare>
bool SkipList<T, Compare>::Impl::erase(const T& value) {
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
