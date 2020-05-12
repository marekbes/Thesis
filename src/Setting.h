#ifndef PROOFOFCONCEPT_SETTING_H
#define PROOFOFCONCEPT_SETTING_H

#include "TaskResult.h"
#include "queries/SmartGrid2.h"
#include "queries/YahooQuery.h"
#include <boost/atomic.hpp>
#include <zconf.h>

class Setting {
public:
  using Query = YahooQuery;
  // Tuple count in a batch
  static const long BATCH_COUNT = 1024 * 16;
  // Batch size in bytes
  static const long BATCH_SIZE =
      BATCH_COUNT * sizeof(typename Query::InputSchema);
  // Number of batches of data
  static long DATA_COUNT;
  static long PAGE_SIZE;
  static int NODES_USED;
  static int THREADS_USED;
  static boost::atomic_int64_t DataCounter;
  static const std::string DATA_PATH;
};

#endif // PROOFOFCONCEPT_SETTING_H
