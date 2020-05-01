#ifndef PROOFOFCONCEPT_RESULTGROUP_H
#define PROOFOFCONCEPT_RESULTGROUP_H

#include "TaskResult.h"
#include <atomic>
#include <set>
#include <tbb/concurrent_unordered_map.h>

struct ResultGroup {
  ResultGroup() : results(2000) {}

  tbb::concurrent_unordered_map<long, tbb::atomic<int32_t>> results;
  std::atomic<long> windowId;
  std::atomic<long> threadSeenCounter;

  void reset();

  friend std::ostream &operator<<(std::ostream &out, const ResultGroup &j);
};

#endif // PROOFOFCONCEPT_RESULTGROUP_H
