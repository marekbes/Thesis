#ifndef PROOFOFCONCEPT_NODECOMM_H
#define PROOFOFCONCEPT_NODECOMM_H

#include "Job.h"
#include "NumaAlloc.h"
#include <atomic>
#include <blockingconcurrentqueue.h>
#include <mutex>
#include <optional>
#include <queue>
#include <tbb/concurrent_priority_queue.h>

class NodeCoordinator;

class NodeComm {
private:
  struct NumaQueueTraits : public moodycamel::ConcurrentQueueDefaultTraits {
    constexpr static const NumaAlloc<char> allocator = NumaAlloc<char>(0);

    static inline void *malloc(size_t size) {
      auto data = allocator.allocate(size + sizeof(size_t));
      *(size_t *)data = size + sizeof(size_t);
      return data + sizeof(size_t);
    }
    static inline void free(void *ptr) {
      auto size = ((size_t *)(ptr))[-1];
      return allocator.deallocate((char *)ptr, size);
    }
  };

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
  NodeCoordinator &coordinator;
  std::vector<NodeComm *, NumaAlloc<NodeComm *>> allComms;
  std::mutex queue_mutex;
  std::atomic<uint32_t> vectorClock[4]{};
  tbb::concurrent_priority_queue<uint32_t, std::greater<>, NumaAlloc<uint32_t>>
      markedAsComplete;

  void SetClock(int node, uint32_t newValue);

public:
  static const int NEXT_NODE = -1;
  NodeComm(int numaNode, NodeCoordinator &coordinator);
  void SendJob(Job &&job, int destinationNode = NEXT_NODE);
  std::optional<Job> TryGetJob();
  void initComms(const std::vector<NodeComm *> &allComms);
  void UpdateClock(uint32_t i);
  bool AllGreaterThan(uint32_t batchId);
  int LowestClock();
};

#endif // PROOFOFCONCEPT_NODECOMM_H
