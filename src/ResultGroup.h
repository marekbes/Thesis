#ifndef PROOFOFCONCEPT_RESULTGROUP_H
#define PROOFOFCONCEPT_RESULTGROUP_H

#include "TaskResult.h"
#include <set>
struct ResultGroup {
  std::set<TaskResult> results;
  void addResult(TaskResult&);
  bool isComplete();
};

#endif // PROOFOFCONCEPT_RESULTGROUP_H