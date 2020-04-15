#ifndef PROOFOFCONCEPT_OBJECTPOOL_H
#define PROOFOFCONCEPT_OBJECTPOOL_H
#include <assert.h>
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>

/**
 * Generic class for object pools.
 */
template <class T, int SZ, class Initialiser, class Releaser>
class StackObjectPool {
public:
  using pointer = std::unique_ptr<T, std::function<void(T *)>>;
  using PoolType = StackObjectPool<T, SZ, Initialiser, Releaser>;

  StackObjectPool() {}
  StackObjectPool(const PoolType &orig) = delete;
  ~StackObjectPool() {}

  /**
   * Acquires an object not being currently used
   * @return pointer to the acquired object
   * @throw std::out_of_range if all the objects inside the pool are being used
   */
  pointer acquire() {
    unsigned int index = 0; // look for the first free object
    auto expected = false;
    while (!m_occupied_registry[index].compare_exchange_weak(expected, true))
      ++index;
    if (index >= SZ)
      throw std::out_of_range("Pool exceeded its size");
    m_initialiser(&m_objects[index]); // initialise it
    // printf("acquiring %p %p %d\n", this,  &m_objects[index], index);
    // return an unique_ptr that calls release when reset
    return pointer(&m_objects[index], [this, index](T *element) -> void {
      release(element, index);
    });
  }

private:
  void release(T *element, unsigned int index) {
#ifdef POC_DEBUG
    std::stringstream stream;
    stream << "Releasing " << element << std::endl;
    std::cout << stream.str();
#endif
    if (&m_objects[index] != element) {
      std::cerr << "no match index and element\n";
      exit(1);
    }
    assert(m_occupied_registry[index]);
    m_occupied_registry[index] = false; // mark the released element as free
    m_releaser(element);                // call release functor
  }

  Initialiser m_initialiser;
  Releaser m_releaser;
  std::atomic_bool m_occupied_registry[SZ]{0};
  T m_objects[SZ];
};

#endif // PROOFOFCONCEPT_OBJECTPOOL_H