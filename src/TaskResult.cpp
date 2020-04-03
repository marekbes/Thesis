#include "TaskResult.h"
std::ostream &operator<<(std::ostream &out, const TaskResult &tr) {
#ifdef POC_DEBUG
  out << "[ windowId: " << tr.windowId << " startPos: " << tr.startPos
      << " endPos: " << tr.endPos << "]";
#endif
  return out;
}