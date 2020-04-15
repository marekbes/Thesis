#include "NodeComm.h"
#include "Setting.h"
#include <iostream>
#include <sstream>

NodeComm::NodeComm(int numaNode, NodeCoordinator &coordinator)
    : NumaNode(numaNode), coordinator(coordinator), queue(100, 32, 32),
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
  if (std::holds_alternative<int>(job))
    throw std::exception();
#ifdef POC_DEBUG
  std::stringstream stream;
  stream << "sending [" << job << "] " << std::dec << NumaNode << " -> "
         << destinationNode << std::endl;
  std::cout << stream.str();
#endif
  if (destinationNode == NEXT_NODE) {
    destinationNode = (NumaNode + 1) % Setting::NODES_USED;
  }
  uint32_t clock[4] = {vectorClock[0], vectorClock[1], vectorClock[2],
                       vectorClock[3]};

  allComms[destinationNode]->queue.try_enqueue(Message(std::move(job), clock));
}

std::optional<Job> NodeComm::TryGetJob() {
  Message msg;
  {
    if (!queue.try_dequeue(msg)) {
      return {};
    }
    if (std::holds_alternative<int>(msg.job))
      throw std::exception();
  }
  for (int i = 0; i < 4; ++i) {
    SetClock(i, msg.vectorClock[i]);
  }
#ifdef POC_DEBUG
  std::stringstream stream;
  stream << "Node " << NumaNode << " sees [" << vectorClock[0] << ","
         << vectorClock[1] << "," << vectorClock[2] << "," << vectorClock[3]
         << "]" << std::endl;
  std::cout << stream.str();
#endif
  return std::move(msg.job);
}

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

  if (clockSnap + Setting::NODES_USED == latestBatch) {
    while (clockSnap + Setting::NODES_USED == latestBatch) {
      localClock.compare_exchange_weak(clockSnap, latestBatch);
    }
    clockSnap = localClock;
    while (true) {
      uint32_t popped;
      if (markedAsComplete.try_pop(popped)) {
        if (clockSnap + Setting::NODES_USED < popped) {
          markedAsComplete.push(popped);
          break;
        }
        while (clockSnap + Setting::NODES_USED == popped) {
          localClock.compare_exchange_weak(clockSnap, popped);
        }
        clockSnap = localClock;
      } else {
        break;
      }
    }
  } else {
    markedAsComplete.push(latestBatch);
  }
}

bool NodeComm::AllGreaterThan(uint32_t batchId) {
  for (int i = 0; i < Setting::NODES_USED; ++i) {
    if (vectorClock[i] <= batchId) {
      return false;
    }
  }
  return true;
}

NodeComm::Message::Message(Job &&j, const uint32_t clock[4])
    : job(std::move(j)) {
  std::copy(clock, clock + 4, vectorClock);
}
NodeComm::Message::Message() : job() {}
