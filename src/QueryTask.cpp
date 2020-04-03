#include "QueryTask.h"

std::ostream &operator<<(std::ostream &out, const QueryTask &qt) {
#ifdef POC_DEBUG
  out << "[ batchId: " << qt.batchId << " startPos: " << qt.startPos
      << " endPos: " << qt.endPos << "]";
#endif
  return out;
}