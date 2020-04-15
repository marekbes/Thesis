#include "QueryTask.h"

std::ostream &operator<<(std::ostream &out, const QueryTask &qt) {
#ifdef POC_DEBUG
  out << "[ batchId: " << qt.batchId
#ifdef POC_DEBUG_POSITION
      << " startPos: " << qt.startPos << " endPos: " << qt.endPos
#endif
      << "]";
#endif
  return out;
}