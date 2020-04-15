#ifndef PROOFOFCONCEPT_RESULTGROUP_H
#define PROOFOFCONCEPT_RESULTGROUP_H

#include "TaskResult.h"
#include <atomic>
#include <set>

struct ResultGroup {
  ResultGroup() : results() {}

  static const int RESULT_COUNT_LIMIT = 3;
  TaskResult results[RESULT_COUNT_LIMIT];
  std::atomic<long> windowId;
  std::atomic<long> resultCount;

  friend std::ostream &operator<<(std::ostream &out, const ResultGroup &j);
};

#endif // PROOFOFCONCEPT_RESULTGROUP_H
