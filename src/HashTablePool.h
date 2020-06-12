#ifndef PROOFOFCONCEPT_HASHTABLEPOOL_H
#define PROOFOFCONCEPT_HASHTABLEPOOL_H
#include "HashTable.h"
#include "ObjectPool.h"
#include "Setting.h"

template <typename KeyT, typename ValueT, typename HashT, typename EqT,
    typename AggrT>
class HashTableInitializer
{
public:
  void operator()(HashTable<KeyT, ValueT, HashT, EqT, AggrT> *element)
  {
    element->clear();
  }
};

template <typename KeyT, typename ValueT, typename HashT, typename EqT,
    typename AggrT>
class HashTableReleaser
{
public:
  void operator()(HashTable<KeyT, ValueT, HashT, EqT, AggrT> *) {}
};

template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>,
    typename EqT = HashMapEqualTo<KeyT>,
    typename AggrT = DummyAggr<ValueT>>
using HashTablePool = StackObjectPool<
        HashTable<KeyT, ValueT, HashT, EqT, AggrT>, Setting::MAP_POOL_SIZE,
HashTableInitializer<KeyT, ValueT, HashT, EqT, AggrT>,
HashTableReleaser<KeyT, ValueT, HashT, EqT, AggrT>>;
#endif // PROOFOFCONCEPT_HASHTABLEPOOL_H
