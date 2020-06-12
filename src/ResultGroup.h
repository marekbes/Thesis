#ifndef PROOFOFCONCEPT_RESULTGROUP_H
#define PROOFOFCONCEPT_RESULTGROUP_H

#include "TaskResult.h"
#include <atomic>
#include <set>
template <typename MergerData, typename MarkerData>
struct ResultGroup {
  ResultGroup(){}

  MergerData mergerData;
  std::atomic<long> windowId;
  MarkerData markerData;

  std::ostream &operator<<(std::ostream &out) {
    out << "ResultGroup [ ";
    out << mergerData.size();
    out << "]";
    return out;
  }

  void reset() {
    markerData.reset();
    mergerData.reset();
    windowId = -1;
  }
};

#endif // PROOFOFCONCEPT_RESULTGROUP_H
