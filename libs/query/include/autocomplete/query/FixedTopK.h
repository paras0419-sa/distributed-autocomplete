#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

namespace autocomplete::query {

// Maintains top-K of T by a strict weak ordering `Less`.
// "Top" = largest under `Less`. Internally a min-heap sized ≤ K.
template <typename T, typename Less>
class FixedTopK {
 public:
  explicit FixedTopK(std::size_t k, Less less = Less{})
      : k_{k}, less_{std::move(less)} {
    heap_.reserve(k);
  }

  void offer(T value) {
    if (k_ == 0) return;
    if (heap_.size() < k_) {
      heap_.push_back(std::move(value));
      std::push_heap(heap_.begin(), heap_.end(), inv_less());
    } else if (less_(heap_.front(), value)) {
      std::pop_heap(heap_.begin(), heap_.end(), inv_less());
      heap_.back() = std::move(value);
      std::push_heap(heap_.begin(), heap_.end(), inv_less());
    }
  }

  // Destructive: returns elements sorted descending under `Less`.
  std::vector<T> take_sorted() {
    std::sort(heap_.begin(), heap_.end(),
              [&](const T& a, const T& b) { return less_(b, a); });
    return std::move(heap_);
  }

 private:
  // std::*_heap builds a max-heap under the supplied compare; we want a
  // min-heap, so invert.
  auto inv_less() const {
    return [this](const T& a, const T& b) { return less_(b, a); };
  }

  std::size_t     k_;
  Less            less_;
  std::vector<T>  heap_;
};

}  // namespace autocomplete::query
