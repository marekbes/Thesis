#ifndef PROOFOFCONCEPT_NODECOMM_H
#define PROOFOFCONCEPT_NODECOMM_H

#include "Job.h"
#include "NumaAlloc.h"
#include <atomic>
#include <mutex>
#include <optional>
#include <tbb/concurrent_priority_queue.h>
#include <queue>

class NodeComm {
private:
  struct Message {
    Message();
    Message(Job &&job, const uint32_t vectorClock[4]);
    Job job;
    uint32_t vectorClock[4]{};

    std::ostream &operator<<(std::ostream &out) {
      out << "[" << vectorClock << "] " << job << std::endl;
      return out;
    }
  };

  int NumaNode;
  std::deque<Message, NumaAlloc<Message>> queue;
  std::vector<NodeComm *, NumaAlloc<NodeComm *>> allComms;
  std::mutex queue_mutex;
  std::atomic<uint32_t> vectorClock[4]{};
  tbb::concurrent_priority_queue<uint32_t, std::greater<>, NumaAlloc<uint32_t>>
      markedAsComplete;

  void SetClock(int node, uint32_t newValue);
public:
  static const int NEXT_NODE = -1;
  NodeComm(int numaNode);
  void SendJob(Job &&job, int destinationNode = NEXT_NODE);
  std::optional<Job> TryGetJob();
  void initComms(const std::vector<NodeComm *> &allComms);
  void UpdateClock(uint32_t i);
  bool AllGreaterThan(uint32_t batchId);
};

#endif // PROOFOFCONCEPT_NODECOMM_H
