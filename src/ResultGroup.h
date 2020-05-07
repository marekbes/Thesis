#ifndef PROOFOFCONCEPT_RESULTGROUP_H
#define PROOFOFCONCEPT_RESULTGROUP_H

#include "TaskResult.h"
#include <atomic>
#include <set>
#include <tbb/concurrent_unordered_map.h>
template <typename TKeyType, typename TValueType>
struct ResultGroup {
  ResultGroup() : results(2000) {}

  tbb::concurrent_unordered_map<TKeyType, TValueType, std::hash<TKeyType>> results;
  std::atomic<long> windowId;
  std::atomic<long> threadSeenCounter;

  std::ostream &operator<<(std::ostream &out) {
    out << "ResultGroup [ ";
    out << results.size();
    out << "]";
    return out;
  }

  void reset() {
    threadSeenCounter = 0;
    for (auto iter = results.begin(); iter != results.end(); iter++) {
      iter->second = 0;
    }
    windowId = -1;
  }
};

#endif // PROOFOFCONCEPT_RESULTGROUP_H
