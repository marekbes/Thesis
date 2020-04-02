#include "NodeCoordinator.h"
#include "Setting.h"
#include "YahooQuery.h"
#include <utility>
template <class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;

NodeCoordinator::NodeCoordinator(int numaNode, void *data)
    : NumaNode(numaNode), InputBuf(data), BatchCounter(0), NodeComms(numaNode),
      parts(NumaAlloc<>(numaNode)) {}

int NodeCoordinator::GetNode() { return NumaNode; }

void NodeCoordinator::ProcessLocalResult(TaskResult result) {
  result.numaNode = NumaNode;
  if (result.isComplete()) {
    OutputResult(result);
  }
  if (result.endingWindow) {
    std::lock_guard<std::mutex> guard(parts_mutex);
    auto iter = parts.find(result.windowId);
    if (iter != parts.end()) {
      auto &other = iter->second;
      YahooQuery::merge(result, other);
      if (result.isComplete()) {
        OutputResult(result);
        parts.erase(result.windowId);
      }
    } else {
      parts[result.windowId] = std::move(result);
    }
  } else {
    NodeComms.SendJob(PassSegmentJob(std::move(result)));
  }
}

void NodeCoordinator::ProcessSegment(PassSegmentJob &segment) {
  auto &result = segment.result;
  std::lock_guard<std::mutex> guard(parts_mutex);
  auto iter = parts.find(result.windowId);
  if (iter != parts.end()) {
    auto &other = iter->second;
    if(other.numaNode != NumaNode)
    {

    }
    MergeResults(other, result);
    if (result.isComplete()) {
      OutputResult(result);
      parts.erase(result.windowId);
    }
  } else {
    parts[result.windowId] = std::move(result);
  }
}

void NodeCoordinator::MergeResults(TaskResult& a, const TaskResult& b)
{
  assert(std::abs(a.maxBatchId - b.maxBatchId) == 1);
  YahooQuery::merge(a, b);
  a.maxBatchId = std::max(a.maxBatchId, b.maxBatchId);
}



void NodeCoordinator::ProcessJob(Job &job) {
  std::visit(
      overload{
          [this](PassSegmentJob &segment){ProcessSegment(segment);},
          [](QueryTask&) {throw std::exception();}
      },
      job);
}

void NodeCoordinator::OutputResult(TaskResult &result) {
  auto &table = result.result;
  for (size_t j = 0; j < table->getNumberOfBuckets(); ++j) {
    auto bucket = table->getBuckets()[j];
    if (bucket.state) {
      *(long *)outputBuf = bucket.key;
      *(long *)outputBuf = bucket.value._1;
    }
  }
}

Job NodeCoordinator::GetJob() {
  auto job = NodeComms.TryGetJob();
  if (job) {
    return std::move(job.value());
  }
  auto batch = BatchCounter.fetch_add(1, boost::memory_order_relaxed);
  auto id = batch * Setting::NODES_USED + GetNode();
  return QueryTask{id,
                   (char *)InputBuf +
                       (batch % Setting::DATA_COUNT) * Setting::BATCH_SIZE,
                   Setting::BATCH_COUNT};
}
NodeComm &NodeCoordinator::GetComm() { return NodeComms; }
