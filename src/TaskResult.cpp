#include "TaskResult.h"
std::ostream &operator<<(std::ostream &out, const TaskResult &tr) {
#ifdef POC_DEBUG

  out << "[ windowId: " << tr.windowId << " batchId: " << tr.batchId
#ifdef POC_DEBUG_POSITION
      << " startPos: " << tr.startPos << " endPos: " << tr.endPos
#endif
      << " result: " << tr.result.get() << "]";
#endif
  return out;
}