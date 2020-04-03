#ifndef PROOFOFCONCEPT_NODECOMM_H
#define PROOFOFCONCEPT_NODECOMM_H

#include "Job.h"
#include "NumaAlloc.h"
#include <mutex>
#include <optional>
#include <queue>

class NodeComm {
private:
  struct Message {
    Message(Job &&job, uint32_t vectorClock[4]);
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
  uint32_t vectorClock[4];

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
