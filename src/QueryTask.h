#ifndef PROOFOFCONCEPT_QUERYTASK_H
#define PROOFOFCONCEPT_QUERYTASK_H

#include "LatencyMonitor.h"
#include <cstdint>
#include <ostream>
struct QueryTask {
  long batchId;
  char *data;
  int size;
  long timestampOffset;
  LatencyMonitor::Timestamp_t latencyMark;
#ifdef POC_DEBUG_POSITION
  uint64_t startPos;
  uint64_t endPos;
#endif
  friend std::ostream &operator<<(std::ostream &out, const QueryTask &qt);
};
#endif // PROOFOFCONCEPT_QUERYTASK_H
