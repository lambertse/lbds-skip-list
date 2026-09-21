#pragma once

#include <cstddef>
#include <functional>
#include <memory>

namespace lbds::skip_list {

namespace internal {
template <typename T>
class ConstIterator;
}  // namespace internal

template <typename T, typename Compare = std::less<T>>
class SkipList {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using value_compare = Compare;

  // Always const iterator - advoid changing data in SkipList
  using const_iterator = internal::ConstIterator<T>;
  using iterator = const_iterator;

  SkipList();
  explicit SkipList(Compare compare);

  ~SkipList();

  SkipList(const SkipList& other) = delete;
  SkipList& operator=(const SkipList& other) = delete;

  SkipList(SkipList&& other) = delete;
  SkipList& operator=(SkipList&& other) = delete;

  [[nodiscard]] bool insert(const T& value);
  [[nodiscard]] bool insert(T&& value);

  [[nodiscard]] bool erase(const T& value);
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;

  // Ordered traversal, ascending under Compare.
  [[nodiscard]] const_iterator begin() const noexcept;
  [[nodiscard]] const_iterator end() const noexcept;
  [[nodiscard]] const_iterator cbegin() const noexcept;
  [[nodiscard]] const_iterator cend() const noexcept;

  [[nodiscard]] const_iterator find(const T& value) const;

  // Range query
  [[nodiscard]] const_iterator lower_bound(const T& value) const;
  [[nodiscard]] const_iterator upper_bound(const T& value) const;

  void display() const;
  void clear() noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace lbds::skip_list

#include <skiplist/detail/skip_list_impl.h>
