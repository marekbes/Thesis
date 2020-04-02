#ifndef PROOFOFCONCEPT_SETTING_H
#define PROOFOFCONCEPT_SETTING_H

#include "TaskResult.h"
#include <boost/atomic.hpp>
#include <zconf.h>
class Setting {

public:
  // Tuple count in a batch
  static const unsigned long BATCH_COUNT = 1024 * 16;
  // Batch size in bytes
  static const unsigned long BATCH_SIZE = BATCH_COUNT * sizeof(InputSchema);
  static const unsigned long DATA_COUNT = 100000;
  static long PAGE_SIZE;
  static int NODES_USED;
  static boost::atomic_int64_t DataCounter;
};

#endif // PROOFOFCONCEPT_SETTING_H