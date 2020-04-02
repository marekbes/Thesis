#ifndef PROOFOFCONCEPT_TASKRESULT_H
#define PROOFOFCONCEPT_TASKRESULT_H

#include "HashTable.h"
#include <functional>
#include <memory>

struct CounterVal {
  long _1;
  long _2;
};

struct InputSchema {
  long timestamp;
  long padding_0;
  long user_id;
  long page_id;
  long ad_id;
  long ad_type;
  long event_type;
  long ip_address;
};

struct TaskResult {
  long windowId;
  bool startingWindow;
  bool endingWindow;
  int numaNode;
  std::unique_ptr<HashTable<long, CounterVal>,
                  std::function<void(HashTable<long, CounterVal> *)>>
      result;
  long batchId;
  [[nodiscard]] bool isComplete() const { return startingWindow && endingWindow; }
#ifdef POC_DEBUG
  long startPos;
  long endPos;
#endif
};

#endif // PROOFOFCONCEPT_TASKRESULT_H
