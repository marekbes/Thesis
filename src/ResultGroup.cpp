#include "ResultGroup.h"

std::ostream &operator<<(std::ostream &out, const ResultGroup &rg) {
  out << "ResultGroup [ ";
  out << rg.results.size();
  out << "]";
  return out;
}

void ResultGroup::reset() {
  threadSeenCounter = 0;
  windowId = -1;
}
