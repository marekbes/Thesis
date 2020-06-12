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
  using MapType = typename PoolType::pointer;
  explicit DefaultTableProvider(TCoordinator &) {}
  DefaultTableProvider(DefaultTableProvider &) {}
  using TResult = TaskResult<typename PoolType::pointer>;
  typename PoolType::pointer acquire(long) { return pool.acquire(); }
};
#endif // PROOFOFCONCEPT_DEFAULTTABLEPROVIDER_H
