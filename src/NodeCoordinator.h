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
#include <vector>

class NodeCoordinator {
  int NumaNode;
  volatile char outputArr[1024];
  volatile void *outputBuf = outputArr;
  void *InputBuf;
  boost::atomic_int BatchCounter;
  NodeComm NodeComms;

  std::mutex parts_mutex;
  std::unordered_map<long, ResultGroup, std::hash<long>, std::equal_to<>,
                     NumaAlloc<std::pair<const long, ResultGroup>>>
      parts;

public:
  explicit NodeCoordinator(int numaNode, void *data);
  NodeComm &GetComm();
  int GetNode();
  Job GetJob();
  void ProcessLocalResult(TaskResult result);
  void ProcessJob(Job &job);

private:
  void OutputResult(TaskResult &result);
  void ProcessSegment(PassSegmentJob &segment);
  void MergeResults(TaskResult &a, const TaskResult &b);
};

#endif // PROOFOFCONCEPT_NODECOORDINATOR_H
