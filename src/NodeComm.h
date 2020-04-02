#ifndef PROOFOFCONCEPT_NODECOMM_H
#define PROOFOFCONCEPT_NODECOMM_H

#include "Job.h"
#include "NumaAlloc.h"
#include <mutex>
#include <queue>
#include <optional>

class NodeComm {
private:
  int NumaNode;
  std::deque<Job, NumaAlloc<Job>> queue;
  // std::vector<std::queue<Job, NumaAlloc<Job>>,
  //            NumaAlloc<std::queue<Job, NumaAlloc<Job>>>> queues;
  std::vector<NodeComm *, NumaAlloc<NodeComm *>> allComms;
  std::mutex queue_mutex;

public:
  static const int NEXT_NODE = -1;
  NodeComm(int numaNode);
  void SendJob(Job &&job, int destinationNode = NEXT_NODE);
  std::optional<Job> TryGetJob();
  void initComms(const std::vector<NodeComm *> &allComms);
};

#endif // PROOFOFCONCEPT_NODECOMM_H
