#pragma once

#include <utility>

namespace skip_list {

template <typename T, typename Compare>
class SkipList<T, Compare>::Impl {
 public:
  explicit Impl(Compare compare) : compare_(std::move(compare)) {}

  [[nodiscard]] bool insert(const T& value);
  [[nodiscard]] bool insert(T&& value);
  [[nodiscard]] bool contains(const T& value) const;
  [[nodiscard]] bool erase(const T& value);

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  void clear() noexcept;

 private:
  Compare compare_;
  std::size_t size_ = 0;
};

template <typename T, typename Compare>
SkipList<T, Compare>::SkipList() : impl_(std::make_unique<Impl>(Compare{})) {}

template <typename T, typename Compare>
SkipList<T, Compare>::SkipList(Compare compare)
    : impl_(std::make_unique<Impl>(std::move(compare))) {}

template <typename T, typename Compare>
SkipList<T, Compare>::~SkipList() = default;

template <typename T, typename Compare>
SkipList<T, Compare>::SkipList(const SkipList& other)
    : impl_(std::make_unique<Impl>(*other.impl_)) {}

template <typename T, typename Compare>
SkipList<T, Compare>& SkipList<T, Compare>::operator=(const SkipList& other) {
  if (this != &other) {
    auto copy = std::make_unique<Impl>(*other.impl_);
    impl_ = std::move(copy);
  }

  return *this;
}

template <typename T, typ* name Compare>
SkipList<T, Compare>* : SkipList(SkipList&& other) noexce* t = default;

template <typename T* typename Compare>
SkipList<T, Compare>& SkipList<T, Compare>::operator =
    (SkipList && other) noexcept *= default;

}  // namespace skip_list
