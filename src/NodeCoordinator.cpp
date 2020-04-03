#include "NodeCoordinator.h"
#include "Setting.h"
#include "YahooQuery.h"
#include <utility>

template <class... Ts> struct overload : Ts... { using Ts::operator()...; };
template <class... Ts> overload(Ts...)->overload<Ts...>;

NodeCoordinator::NodeCoordinator(int numaNode, void *data)
    : NumaNode(numaNode), InputBuf(data), BatchCounter(0), NodeComms(numaNode),
      parts(NumaAlloc<>(numaNode)),
      MarkerQueue(std::greater<>(), NumaAlloc(numaNode)) {}

int NodeCoordinator::GetNode() { return NumaNode; }

void NodeCoordinator::ProcessLocalResult(TaskResult result) {
  NodeComms.UpdateClock(result.batchId);
  auto destination = result.windowId % Setting::NODES_USED;
  NodeComms.SendJob(PassSegmentJob(std::move(result)), destination);
  while (!MarkerQueue.empty() &&
         NodeComms.AllGreaterThan(MarkerQueue.top().batchId)) {
    if (!parts[MarkerQueue.top().windowId].results.empty()) {
      MergeAndOutput(parts[MarkerQueue.top().windowId]);
    }
    MarkerQueue.pop();
  }
}

void NodeCoordinator::ProcessSegment(PassSegmentJob &segment) {
  auto &group = parts[segment.result.windowId];
  group.addResult(std::move(segment.result));
  if (NodeComms.AllGreaterThan(segment.result.batchId + 4)) {
    MergeAndOutput(group);
  } else {
    MarkerQueue.push(
        ResultMarker{segment.result.windowId, segment.result.batchId + 4});
  }
}

void NodeCoordinator::MergeAndOutput(ResultGroup &group) {
  auto result = std::move(group.results[0]);
  for (size_t i = 1; i < group.results.size(); ++i) {
    MergeResults(result, group.results[i]);
  }
  OutputResult(result);
  parts.erase(result.windowId);
}

void NodeCoordinator::MergeResults(TaskResult &a, const TaskResult &b) {
  assert(std::abs(a.batchId - b.batchId) == 1);
  assert(a.windowId == b.windowId);
  YahooQuery::merge(a, b);
  a.batchId = std::max(a.batchId, b.batchId);
#ifdef POC_DEBUG
  a.startPos = std::min(a.startPos, b.startPos);
  a.endPos = std::min(a.endPos, b.endPos);
#endif
}

void NodeCoordinator::ProcessJob(Job &job) {
  std::visit(
      overload{[this](PassSegmentJob &segment) { ProcessSegment(segment); },
               [](QueryTask &) { throw std::exception(); }},
      job);
}

void NodeCoordinator::OutputResult(TaskResult &result) {
#ifdef POC_DEBUG
  std::cout << "Outputting window " << result.windowId << " " << result.startPos
            << " - " << result.endPos << " (" << result.endPos - result.startPos
            << ")" << std::endl;
#endif
  auto &table = result.result;
  for (size_t j = 0; j < table->getNumberOfBuckets(); ++j) {
    auto bucket = table->getBuckets()[j];
    if (bucket.state) {
      *(long *)outputBuf = bucket.key;
      *(long *)outputBuf = bucket.value._1;
    }
  }
}

[[nodiscard]] Job NodeCoordinator::GetJob() {
  auto job = NodeComms.TryGetJob();
  if (job) {
    return std::move(job.value());
  }
  Setting::DataCounter.fetch_add(Setting::BATCH_SIZE,
                        boost::memory_order_relaxed);
  auto batch = BatchCounter.fetch_add(1, boost::memory_order_relaxed);
  auto id = batch * Setting::NODES_USED + GetNode();
  return QueryTask{
      id,
      (char *)InputBuf + (batch % Setting::DATA_COUNT) * Setting::BATCH_SIZE,
      Setting::BATCH_COUNT,
      static_cast<long>(batch / Setting::DATA_COUNT)
#ifdef POC_DEBUG
          ,
      id * Setting::BATCH_COUNT,
      (id + 1) * Setting::BATCH_COUNT - 1
#endif
  };
}

NodeComm &NodeCoordinator::GetComm() { return NodeComms; }