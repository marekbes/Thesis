#ifndef PROOFOFCONCEPT_EAGERRESULTMERGER_H
#define PROOFOFCONCEPT_EAGERRESULTMERGER_H

#include <tbb/concurrent_unordered_map.h>
#include <cassert>
class TCoordinator {
  class group;
  using RGroup = group;
};

template <typename TQuery, typename TCoordinator> class EagerResultMerger {
  TCoordinator &coordinator;

public:
  struct ResultGroupData {
    tbb::concurrent_unordered_map<typename TQuery::KeyType,
                                  typename TQuery::GlobalValueType,
                                  std::hash<typename TQuery::KeyType>>
        results;

    ResultGroupData() : results(200) {}

    void reset() {
       results.clear();
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
        while (true) {
          auto where = groupTable.find(buckets[i].key);
          std::pair<
              typename tbb::concurrent_unordered_map<
                  typename TQuery::KeyType, std::atomic<int32_t>>::iterator,
              bool>
              where2;
          auto &value =
              where == groupTable.end()
                  ? groupTable.emplace(buckets[i].key, 0).first->second
                  : (*where).second;
          value += buckets[i].value._1;
          break;
        }
      }
    }
  }

  void Output(ResultGroupData &) {};
};

#endif // PROOFOFCONCEPT_EAGERRESULTMERGER_H
