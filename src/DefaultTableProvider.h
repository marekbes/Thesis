#ifndef PROOFOFCONCEPT_DEFAULTTABLEPROVIDER_H
#define PROOFOFCONCEPT_DEFAULTTABLEPROVIDER_H

#include "HashTablePool.h"
#include "TaskResult.h"

template <template <typename> typename TQuery, typename TCoordinator>
class DefaultTableProvider {
  using Query = TQuery<DefaultTableProvider>;
  using PoolType =
      HashTablePool<typename Query::KeyType, typename Query::LocalValueType>;
  HashTablePool<typename Query::KeyType, typename Query::LocalValueType> pool;

public:
  explicit DefaultTableProvider(TCoordinator &) {}
  DefaultTableProvider(DefaultTableProvider &) {}
  using TResult = TaskResult<std::unique_ptr<
      HashTable<typename Query::KeyType, typename Query::LocalValueType>,
      std::function<void(HashTable<typename Query::KeyType,
                                   typename Query::LocalValueType> *)>>>;
  typename PoolType::pointer acquire(long) { return pool.acquire(); }
};
#endif // PROOFOFCONCEPT_DEFAULTTABLEPROVIDER_H
