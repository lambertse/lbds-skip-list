#pragma once

#include <cstddef>
#include <functional>
#include <memory>

namespace lbds::skip_list {

template <typename T, typename Compare = std::less<T>>
class SkipList {
 public:
  SkipList();
  explicit SkipList(Compare compare);

  ~SkipList();

  SkipList(const SkipList& other) = delete;
  SkipList& operator=(const SkipList& other) = delete;

  SkipList(SkipList&& other) = delete;
  SkipList& operator=(SkipList&& other) = delete;

  // Return true when the value was inserted, false when an equivalent value
  // (under Compare) was already present.
  [[nodiscard]] bool insert(const T& value);
  [[nodiscard]] bool insert(T&& value);

  [[nodiscard]] bool contains(const T& value) const;
  [[nodiscard]] bool erase(const T& value);

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;

  void display() const;
  void clear() noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace lbds::skip_list

#include <skiplist/detail/skip_list_impl.h>
