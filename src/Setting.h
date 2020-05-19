#ifndef PROOFOFCONCEPT_SETTING_H
#define PROOFOFCONCEPT_SETTING_H

#include "TaskResult.h"
#include <boost/atomic.hpp>
#include <zconf.h>

class Setting {
public:
  // Tuple count in a batch
  static const long BATCH_COUNT = 1024 * 16;
  // Batch size in bytes
  static long BATCH_SIZE;
  // Number of batches of data
  static long DATA_COUNT;
  static long PAGE_SIZE;
  static int NODES_USED;
  static int THREADS_USED;
  static boost::atomic_int64_t DataCounter;
  static const std::string DATA_PATH;
};

#endif // PROOFOFCONCEPT_SETTING_H
