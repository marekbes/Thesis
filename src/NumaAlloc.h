#ifndef PROOFOFCONCEPT_NUMAALLOC_H
#define PROOFOFCONCEPT_NUMAALLOC_H
/* (C) Copyright Reid Atcheson 2016.
 * Permission to copy, use, modify, sell and distribute this software
 * is granted provided this copyright notice appears in all copies.
 * This software is provided "as is" without express or implied
 * warranty, and with no claim as to its suitability for any purpose.
 */
#include <new>
#include <numa.h>
#include <type_traits>

template <class T = int>
class NumaAlloc {
  const int node;

public:
  typedef T value_type;
  typedef std::true_type propagate_on_container_move_assignment;
  typedef std::true_type is_always_equal;

  template <class U>
  constexpr NumaAlloc(const NumaAlloc<U> &other) noexcept : node(other.GetNode()) {}
  constexpr NumaAlloc(const int node) noexcept : node(node) {};
  template<typename U> struct rebind { typedef NumaAlloc<U> other;};

  [[nodiscard]] T *allocate(const size_t num) const {

    auto ret = numa_alloc_onnode(num * sizeof(T), node);
    if (!ret)
      throw std::bad_alloc();
    return reinterpret_cast<T *>(ret);
  }

  void deallocate(T *p, size_t num) noexcept { numa_free(p, num * sizeof(T)); }

  [[nodiscard]] int GetNode() const {
    return node;
  }
};

template <class T1, class T2>
bool operator==(const NumaAlloc<T1> &, const NumaAlloc<T2> &) noexcept {
  return true;
}

template <class T1, class T2>
bool operator!=(const NumaAlloc<T1> &, const NumaAlloc<T2> &) noexcept {
  return false;
}
#endif // PROOFOFCONCEPT_NUMAALLOC_H
