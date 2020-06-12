#ifndef PROOFOFCONCEPT_DELAYEDRESULTMERGER_H
#define PROOFOFCONCEPT_DELAYEDRESULTMERGER_H

#include <atomic>
#include <cassert>
#include <iostream>
#include <sstream>
#include <tbb/concurrent_unordered_map.h>
#include "SlidingWindowHandler.h"

template <typename TQuery, typename TCoordinator> class DelayedResultMerger {
  TCoordinator &coordinator;

private:
  void MergeResults(typename TQuery::TResult &a,
                    const typename TQuery::TResult &b) {
    assert(a.windowId == b.windowId);
#ifdef POC_DEBUG
    std::stringstream stream;
    stream << "Merging windowId: " << a.windowId << " "
#ifdef POC_DEBUG_POSITION
           << a.startPos << " - " << a.endPos << " (" << a.endPos - a.startPos
           << ")"
#endif
           << std::endl;
    std::cout << stream.str();
#endif
    TQuery::merge(a, b);
    a.batchId = std::max(a.batchId, b.batchId);
#ifdef POC_DEBUG_POSITION
    a.startPos = std::min(a.startPos, b.startPos);
    a.endPos = std::max(a.endPos, b.endPos);
#endif
  }

public:
  struct ResultGroupData {
    static const int RESULT_COUNT_LIMIT = 20;

    typename TQuery::TResult results[RESULT_COUNT_LIMIT];
    std::atomic<long> resultCount;

    ResultGroupData() {}
    void reset() {
      for (int i = 0; i < RESULT_COUNT_LIMIT; ++i) {
        results[i].result = nullptr;
      }
      resultCount = 0;
    }
  };

  explicit DelayedResultMerger(TCoordinator &coordinator)
      : coordinator(coordinator) {}
  void AddResults(typename TQuery::TResult &&result) {
#ifdef POC_DEBUG_POSITION
    assert(result.batchId <= result.windowId * 2);
    assert(result.endPos - result.startPos <= 9999);
#endif
    auto &group = coordinator.GetResultGroup(result.windowId);
    coordinator.MarkGroupWithWindowId(group, result.windowId);
    auto &data = group.mergerData;
    auto position = data.resultCount.fetch_add(1);
    assert(position < ResultGroupData::RESULT_COUNT_LIMIT);
    data.results[position] = std::move(result);
  }

  typename SlidingWindowHandler<TQuery>::SliderResult Output(ResultGroupData &group) {
    int count = group.resultCount;
#ifdef POC_LATENCY
    auto timestamp = group.results[0].latencyMark;
#endif
    for (auto i = 1; i < count; ++i) {
      MergeResults(group.results[0], group.results[i]);
#ifdef POC_LATENCY
      timestamp = std::min(timestamp, group.results[i].latencyMark);
#endif
    }
#ifdef POC_DEBUG_POSITION
    assert(group.results[0].endPos - group.results[0].startPos == 9999);
#endif
    return {std::move(group.results[0].result)
#ifdef POC_LATENCY
                                          ,
                                      timestamp
#endif
    };
  };
};
#endif // PROOFOFCONCEPT_DELAYEDRESULTMERGER_H