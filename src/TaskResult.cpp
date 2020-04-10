#include "TaskResult.h"
std::ostream &operator<<(std::ostream &out, const TaskResult &tr) {
#ifdef POC_DEBUG

  out << "[ windowId: " << tr.windowId << " batchId: " << tr.batchId
      << " startPos: " << tr.startPos << " endPos: " << tr.endPos
      << " result: " << tr.result.get() << "]";
#endif
  return out;
}