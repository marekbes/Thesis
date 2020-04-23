#include "ResultGroup.h"

std::ostream &operator<<(std::ostream &out, const ResultGroup &rg) {
  out << "ResultGroup [ ";
  for (const auto &result : rg.results) {
    out << result << ", ";
  }
  out << "]";
  return out;
}

void ResultGroup::reset() {
  resultCount = 0;
  threadSeenCounter = 0;
  windowId = -1;
}
