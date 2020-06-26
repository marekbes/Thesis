#ifndef PROOFOFCONCEPT_DIRECTRESULTMERGER_H
#define PROOFOFCONCEPT_DIRECTRESULTMERGER_H

#include "SlidingWindowHandler.h"
#include <cassert>
#include <tbb/concurrent_unordered_map.h>

template <template <typename> typename TQuery, typename TCoordinator>
struct DirectTableProvider {
  using Query = TQuery<DirectTableProvider>;
  using MapType =
      typename HashTablePool<typename Query::KeyType,
                             typename Query::LocalValueType>::pointer;
  struct TableProxy {
    typename TCoordinator::RGroup *group;
    long windowId;

    explicit TableProxy(typename TCoordinator::RGroup *group) : group(group) {}

    void insert_or_modify(const typename Query::KeyType &key,
                          const typename Query::LocalValueType &value, long) {
      auto &groupTable = group->mergerData.results;
      auto where = groupTable.find(key);
      auto &counter = where == groupTable.end()
                          ? groupTable.emplace(key, 0).first->second
                          : (*where).second;
      counter += value._1;
    }

    TableProxy *operator->() { return this; }
  };
  using TResult = TaskResult<TableProxy>;
  TCoordinator &coordinator;
  DirectTableProvider(DirectTableProvider &provider)
      : coordinator(provider.coordinator) {}
  explicit DirectTableProvider(TCoordinator &coordinator)
      : coordinator(coordinator) {}

  TableProxy acquire(long windowId) {
    auto& group = coordinator.GetResultGroup(windowId);
    coordinator.MarkGroupWithWindowId(group, windowId);
    return TableProxy(&group);
  }
};

template <typename TQuery, typename TCoordinator> class DirectResultMerger {
  TCoordinator &coordinator;
  HashTablePool<typename TQuery::KeyType, typename TQuery::LocalValueType> pool;

public:
  struct ResultGroupData {
    tbb::concurrent_unordered_map<typename TQuery::KeyType,
                                  typename TQuery::GlobalValueType,
                                  std::hash<typename TQuery::KeyType>>
        results;
    std::atomic<LatencyMonitor::Timestamp_t> latencyMark;

    ResultGroupData()
        : results(Setting::AGGREGATE_KEYS), latencyMark(LatencyMonitor::Timestamp_t::max()) {}

    void reset() {
      results.clear();
      latencyMark = LatencyMonitor::Timestamp_t::max();
    }
  };

  explicit DirectResultMerger(TCoordinator &coordinator)
      : coordinator(coordinator) {}

  void AddResults(typename TQuery::TResult &&result) {
#ifdef POC_DEBUG_POSITION
    assert(result.endPos - result.startPos <= 99999);
#endif
    auto &group = coordinator.GetResultGroup(result.windowId);
    auto snapLatencyMark = group.mergerData.latencyMark.load();
    while (snapLatencyMark > result.latencyMark) {
      group.mergerData.latencyMark.compare_exchange_weak(snapLatencyMark,
                                                         result.latencyMark);
    }
  }

  typename SlidingWindowHandler<TQuery>::SliderResult
  Output(ResultGroupData &group) {
    auto map = pool.acquire();
    for (auto i = group.results.cbegin(); i != group.results.cend(); i++) {
      map->insert(i->first, TQuery::ConvertToLocal(i->second), 0);
    }
    return {std::move(map)
#ifdef POC_LATENCY
                ,
            group.latencyMark
#endif
    };
  }
};

#endif // PROOFOFCONCEPT_DIRECTRESULTMERGER_H
