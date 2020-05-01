#ifndef PROOFOFCONCEPT_NODECOORDINATOR_H
#define PROOFOFCONCEPT_NODECOORDINATOR_H

#include "Job.h"
#include "NodeComm.h"
#include "NumaAlloc.h"
#include "QueryTask.h"
#include "ResultGroup.h"
#include "TaskResult.h"
#include <boost/atomic.hpp>
#include <hash_fun.h>
#include <queue>
#include <tbb/concurrent_priority_queue.h>
#include <tbb/concurrent_unordered_map.h>
#include <vector>

class NodeCoordinator {
  int NumaNode;
  volatile char outputArr[1024];
  volatile void *outputBuf = outputArr;
  void *InputBuf;
  boost::atomic_int BatchCounter;
  std::vector<NodeCoordinator *, NumaAlloc<NodeCoordinator *>> coordinators;

  static const int PARTS_COUNT = 200;
  static ResultGroup parts[PARTS_COUNT];

public:
  NodeComm NodeComms;
  explicit NodeCoordinator(int numaNode, void *data);
  int GetNode();
  QueryTask GetJob();
  void ProcessLocalResult(TaskResult &&result);
  void markWindowDone(long i);
  void SetCoordinators(std::vector<NodeCoordinator *> coordinators);

};

#endif // PROOFOFCONCEPT_NODECOORDINATOR_H
