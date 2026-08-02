#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

namespace skip_list {

template <typename T, typename Compare = std::less<T>>
class SkipList {
 public:
  SkipList();
  explicit SkipList(Compare compare);

  ~SkipList();

  SkipList(const SkipList& other);
  SkipList& operator=(const SkipList& other);

  SkipList(SkipList&& other) noexcept;
  SkipList& operator=(SkipList&& other) noexcept;

  [[nodiscard]] bool insert(const T& value);
  [[nodiscard]] bool insert(T&& value);

  [[nodiscard]] bool contains(const T& value) const;
  [[nodiscard]] bool erase(const T& value);

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;

  void clear() noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace skip_list

#include <skiplist/detail/skip_list_impl.h>
