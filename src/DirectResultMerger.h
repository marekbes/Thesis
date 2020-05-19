#ifndef PROOFOFCONCEPT_DIRECTRESULTMERGER_H
#define PROOFOFCONCEPT_DIRECTRESULTMERGER_H

#include <cassert>
#include <tbb/concurrent_unordered_map.h>

template <template <typename> typename TQuery, typename TCoordinator>
struct DirectTableProvider {
  using Query = TQuery<DirectTableProvider>;
  struct TableProxy {
    typename TCoordinator::RGroup &group;

    explicit TableProxy(typename TCoordinator::RGroup &group) : group(group) {}
    TableProxy &operator=(TableProxy &&) noexcept { return *this; };
    constexpr TableProxy(const TableProxy &prox) noexcept : group(prox.group) {}

    void insert_or_modify(const typename Query::KeyType &key,
                          const typename Query::LocalValueType &value, long) {
      auto &groupTable = group.mergerData.results;
      auto where = groupTable.find(key);
      auto &counter = where == groupTable.end()
                          ? groupTable.emplace(key, 0).first->second
                          : (*where).second;
      counter += value._1;
    }

    TableProxy* operator->() {
      return this;
    }
  };
  using TResult = TaskResult<TableProxy>;
  TCoordinator &coordinator;
  DirectTableProvider(DirectTableProvider &provider)
      : coordinator(provider.coordinator) {}
  explicit DirectTableProvider(TCoordinator &coordinator)
      : coordinator(coordinator) {}

  TableProxy acquire(long windowId) {
    return TableProxy(coordinator.GetResultGroup(windowId));
  }
};

template <typename TQuery, typename TCoordinator> class DirectResultMerger {
  TCoordinator &coordinator;

public:
  struct ResultGroupData {
    tbb::concurrent_unordered_map<typename TQuery::KeyType,
                                  typename TQuery::GlobalValueType,
                                  std::hash<typename TQuery::KeyType>>
        results;
    std::atomic<LatencyMonitor::Timestamp_t> latencyMark;

    ResultGroupData()
        : results(200), latencyMark(LatencyMonitor::Timestamp_t::max()) {}

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

  void Output(ResultGroupData &group) {
#ifdef POC_LATENCY
    LatencyMonitor::InsertMeasurement(group.latencyMark);
#endif
  };
};

#endif // PROOFOFCONCEPT_DIRECTRESULTMERGER_H
