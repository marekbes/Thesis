#ifndef PROOFOFCONCEPT_TASKRESULT_H
#define PROOFOFCONCEPT_TASKRESULT_H

#include "HashTable.h"
#include <functional>
#include <memory>

template <typename KeyType, typename TValue> struct TaskResult {
  long windowId;
  bool startingWindow;
  bool endingWindow;
  int numaNode;
  std::unique_ptr<HashTable<KeyType, TValue>,
                  std::function<void(HashTable<KeyType, TValue> *)>>
      result;
  long batchId;
#ifdef POC_DEBUG_POSITION
  uint64_t startPos;
  uint64_t endPos;
#endif

  std::ostream &operator<<(std::ostream &out) {
#ifdef POC_DEBUG
    out << "[ windowId: " << windowId << " batchId: " << batchId
#ifdef POC_DEBUG_POSITION
        << " startPos: " << startPos << " endPos: " << endPos
#endif
        << " result: " << result.get() << "]";
#endif
    return out;
  }
};
#endif // PROOFOFCONCEPT_TASKRESULT_H
