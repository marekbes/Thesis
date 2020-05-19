#ifndef PROOFOFCONCEPT_EAGERRESULTMERGER_H
#define PROOFOFCONCEPT_EAGERRESULTMERGER_H

#include <cassert>
#include <tbb/concurrent_unordered_map.h>

template <typename TQuery, typename TCoordinator> class EagerResultMerger {
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

  explicit EagerResultMerger(TCoordinator &coordinator)
      : coordinator(coordinator) {}
  void AddResults(typename TQuery::TResult &&result) {
#ifdef POC_DEBUG_POSITION
    assert(result.endPos - result.startPos <= 99999);
#endif
    auto &group = coordinator.GetResultGroup(result.windowId);
    coordinator.MarkGroupWithWindowId(group, result.windowId);
    auto table = result.result.get();
    auto buckets = table->getBuckets();
    for (size_t i = 0; i < table->getNumberOfBuckets(); ++i) {
      if (buckets[i].state) {
        auto &groupTable = group.mergerData.results;
        auto where = groupTable.find(buckets[i].key);
        auto &value = where == groupTable.end()
                          ? groupTable.emplace(buckets[i].key, 0).first->second
                          : (*where).second;
        value += buckets[i].value._1;
      }
    }
    auto snapLatencyMark = group.mergerData.latencyMark.load();
    while (snapLatencyMark > result.latencyMark) {
      group.mergerData.latencyMark.compare_exchange_weak(snapLatencyMark,
                                                         result.latencyMark);
    }
  }

  void Output(ResultGroupData &group){
#ifdef POC_LATENCY
    LatencyMonitor::InsertMeasurement(group.latencyMark);
#endif
  };
};

#endif // PROOFOFCONCEPT_EAGERRESULTMERGER_H
