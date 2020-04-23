#include "NodeComm.h"
#include "NodeCoordinator.h"
#include "Setting.h"
#include <iostream>
#include <sstream>

NodeComm::NodeComm(int numaNode, NodeCoordinator &coordinator)
    : NumaNode(numaNode), coordinator(coordinator),
      allComms(NumaAlloc<Job>(numaNode)),
      markedAsComplete(NumaAlloc(numaNode)) {
  for (auto &i : vectorClock) {
    i = 0;
  }
  vectorClock[numaNode] = numaNode;
}

void NodeComm::initComms(const std::vector<NodeComm *> &comms) {
  this->allComms.assign(comms.begin(), comms.end());
}

void NodeComm::SendJob(Job &&job, int destinationNode) {
#ifdef POC_DEBUG
  std::stringstream stream;
  stream << "sending [" << job << "] " << std::dec << NumaNode << " -> "
         << destinationNode << std::endl;
  std::cout << stream.str();
#endif
  if (destinationNode == NEXT_NODE) {
    destinationNode = (NumaNode + 1) % Setting::NODES_USED;
  }
  for (int i = 0; i < 4; ++i) {
    if (i != destinationNode) {
      allComms[destinationNode]->SetClock(i, vectorClock[i]);
    }
  }
  // allComms[destinationNode]->queue.try_enqueue(Message(std::move(job),
  // clock));
}

std::optional<Job> NodeComm::TryGetJob() { return {}; }

void NodeComm::SetClock(int node, uint32_t newValue) {
  auto value = static_cast<uint32_t>(vectorClock[node]);
  while (value < newValue) {
    vectorClock[node].compare_exchange_weak(value, newValue,
                                            std::memory_order_relaxed);
  }
}

void NodeComm::UpdateClock(uint32_t latestBatch) {
  auto &localClock = vectorClock[NumaNode];
  uint32_t clockSnap = localClock;
  if (clockSnap >= latestBatch) {
    return;
  }

#ifdef POC_DEBUG_COMM
  {
    std::stringstream stream;
    stream << "Node " << NumaNode << " starting marker: " << latestBatch
           << " localClock: " << static_cast<uint32_t>(localClock) << std::endl;
    std::cout << stream.str();
  }
#endif

  if (clockSnap + Setting::NODES_USED == latestBatch) {
    while (clockSnap + Setting::NODES_USED == latestBatch) {
      localClock.compare_exchange_weak(clockSnap, latestBatch);
    }
    clockSnap = localClock;
  } else {
#ifdef POC_DEBUG_COMM
    {
      std::stringstream stream;
      stream << "Node " << NumaNode << " returning marker: " << latestBatch
             << " localClock: " << static_cast<uint32_t>(localClock)
             << std::endl;
      std::cout << stream.str();
    }
#endif
    markedAsComplete.push(latestBatch);
  }
  while (true) {
    uint32_t popped;
    if (markedAsComplete.try_pop(popped)) {
#ifdef POC_DEBUG_COMM
      {
        std::stringstream stream;
        stream << "Node " << NumaNode << " popped marker: " << popped
               << " localClock: " << static_cast<uint32_t>(localClock)
               << std::endl;
        std::cout << stream.str();
      }
#endif
      if (clockSnap + Setting::NODES_USED < popped) {
#ifdef POC_DEBUG_COMM
        {
          std::stringstream stream;
          stream << "Node " << NumaNode
                 << " returning popped marker: " << popped
                 << " localClock: " << static_cast<uint32_t>(localClock)
                 << std::endl;
          std::cout << stream.str();
        }
#endif
        markedAsComplete.push(popped);
        break;
      }
      while (localClock + Setting::NODES_USED == popped) {
        localClock.compare_exchange_weak(clockSnap, popped);
      }
      clockSnap = localClock;
    } else {
      break;
    }
  }
#ifdef POC_DEBUG_COMM
  {
    std::stringstream stream;
    stream << "Node " << NumaNode << " finished marker: " << latestBatch
           << " localClock: " << static_cast<uint32_t>(localClock) << std::endl;
    std::cout << stream.str();
  }
#endif
}

bool NodeComm::AllGreaterThan(uint32_t batchId) {
  for (int i = 0; i < Setting::NODES_USED; ++i) {
    if (vectorClock[i] <= batchId) {
      return false;
    }
  }
  return true;
}
int NodeComm::LowestClock() {
  int lowest = vectorClock[0];
  for (int i = 1; i < Setting::NODES_USED; ++i) {
    lowest = std::min(lowest, static_cast<int>(vectorClock[i]));
  }
  return lowest;
}

NodeComm::Message::Message(Job &&j, const uint32_t clock[4])
    : job(std::move(j)) {
  std::copy(clock, clock + 4, vectorClock);
}
NodeComm::Message::Message() : job() {}
