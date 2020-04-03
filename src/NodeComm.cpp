//
// Created by mb4416 on 26/03/2020.
//

#include "NodeComm.h"
#include "Setting.h"
#include <iostream>
NodeComm::NodeComm(int numaNode)
    : NumaNode(numaNode), queue(NumaAlloc<Job>(numaNode)),
      allComms(NumaAlloc<Job>(numaNode)) {}

void NodeComm::initComms(const std::vector<NodeComm *> &comms) {
  this->allComms.assign(comms.begin(), comms.end());
  for (size_t i = 0; i < comms.size(); ++i) {
    vectorClock[i] = 0;
  }
}

void NodeComm::SendJob(Job &&job, int destinationNode) {
  //std::cout << job << " " << NumaNode << " -> " << destinationNode << std::endl;
  if (destinationNode == NEXT_NODE) {
    destinationNode = (NumaNode + 1) % Setting::NODES_USED;
  }
  std::lock_guard<std::mutex> guard(allComms[destinationNode]->queue_mutex);
  allComms[destinationNode]->queue.push_front(
      Message(std::move(job), vectorClock));
}
std::optional<Job> NodeComm::TryGetJob() {
  std::lock_guard<std::mutex> guard(queue_mutex);
  if (queue.empty()) {
    return {};
  }
  auto message = std::move(queue.back());
  for (int i = 0; i < 4; ++i) {
    vectorClock[i] = std::max(vectorClock[i], message.vectorClock[i]);
  }
  queue.pop_back();
  return std::move(message.job);
}

void NodeComm::UpdateClock(uint32_t latestBatch) {
  std::lock_guard<std::mutex> guard(queue_mutex);
  vectorClock[NumaNode] = latestBatch;
}

bool NodeComm::AllGreaterThan(uint32_t batchId) {
  for (int i = 0; i < Setting::NODES_USED; ++i) {
    if (vectorClock[i] <= batchId) {
      return false;
    }
  }
  return true;
}

NodeComm::Message::Message(Job &&j, uint32_t clock[4]) : job(std::move(j)) {
  std::copy(clock, clock + 4, vectorClock);
}
