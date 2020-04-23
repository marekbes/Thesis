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

  static const int PARTS_COUNT = 1000;
  std::vector<ResultGroup, NumaAlloc<ResultGroup>> parts;

  struct ResultMarker {
    long windowId;
    long batchId;

    bool operator<(const ResultMarker &rhs) const {
      return batchId < rhs.batchId;
    }
    bool operator>(const ResultMarker &rhs) const {
      return batchId > rhs.batchId;
    }
  };
  tbb::concurrent_priority_queue<ResultMarker, std::greater<>,
                                 NumaAlloc<ResultMarker>>
      MarkerQueue;
  std::set<long, std::less<>, NumaAlloc<long>> MarkerSet;

public:
  NodeComm NodeComms;
  explicit NodeCoordinator(int numaNode, void *data);
  int GetNode();
  Job GetJob();
  void ProcessLocalResult(TaskResult &&result);
  void ProcessJob(Job &job);

private:
  void OutputResult(const TaskResult &result);
  void ProcessSegment(PassSegmentJob &segment);
  static void MergeResults(TaskResult &a, const TaskResult &b);
  void MergeAndOutput(size_t resultCount,
                      TaskResult results[ResultGroup::RESULT_COUNT_LIMIT]);
};

#endif // PROOFOFCONCEPT_NODECOORDINATOR_H
