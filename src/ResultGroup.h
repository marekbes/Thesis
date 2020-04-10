#ifndef PROOFOFCONCEPT_RESULTGROUP_H
#define PROOFOFCONCEPT_RESULTGROUP_H

#include "TaskResult.h"
#include <set>
struct ResultGroup {
  ResultGroup(): results(){}

  std::vector<TaskResult> results;
  void addResult(TaskResult&&);
  bool isComplete();

  friend std::ostream &operator<<(std::ostream &out, const ResultGroup &j);
};

#endif // PROOFOFCONCEPT_RESULTGROUP_H
