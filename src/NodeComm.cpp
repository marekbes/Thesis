//
// Created by mb4416 on 26/03/2020.
//

#include "NodeComm.h"
#include "Setting.h"
NodeComm::NodeComm(int numaNode)
    : NumaNode(numaNode), queue(NumaAlloc<Job>(numaNode)), allComms(NumaAlloc<Job>(numaNode)) {}

void NodeComm::initComms(const std::vector<NodeComm *> &comms) {
  this->allComms.assign(comms.begin(), comms.end());
}

void NodeComm::SendJob(Job &&job, int destinationNode) {
  if (destinationNode == NEXT_NODE) {
    destinationNode = (NumaNode + 1) % Setting::NODES_USED;
  }
  std::lock_guard<std::mutex> guard(queue_mutex);
  allComms[destinationNode]->queue.push_front(std::move(job));
}
std::optional<Job>  NodeComm::TryGetJob() {
  std::lock_guard<std::mutex> guard(queue_mutex);
  if (queue.empty()) {
    return {};
  }
  std::optional<Job> job = std::move(queue.back());
  queue.pop_back();
  return job;
}