#ifndef PROOFOFCONCEPT_SETTING_H
#define PROOFOFCONCEPT_SETTING_H

#include "TaskResult.h"
#include <boost/atomic.hpp>
#include <zconf.h>

class Setting {
public:
  // Tuple count in a batch
  static long BATCH_COUNT;
  // Batch size in bytes
  static long BATCH_SIZE;
  // Number of batches of data
  static long DATA_COUNT;
  static long PAGE_SIZE;
  static int NODES_USED;
  static int THREADS_USED;
  static boost::atomic_int64_t DataCounter;
  static const std::string DATA_PATH;
  static int WINDOW_SLIDE;
  static int WINDOW_SIZE;
  static const int MAP_POOL_SIZE = 1024 * 16;
  static int SHARED_SLOTS;
};

#endif // PROOFOFCONCEPT_SETTING_H
