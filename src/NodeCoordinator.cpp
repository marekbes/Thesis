#include "NodeCoordinator.h"
#include "Setting.h"
#include "YahooQuery.h"
#include <sstream>
#include <utility>

template <class... Ts> struct overload : Ts... { using Ts::operator()...; };
template <class... Ts> overload(Ts...)->overload<Ts...>;

NodeCoordinator::NodeCoordinator(int numaNode, void *data)
    : NumaNode(numaNode), InputBuf(data), BatchCounter(0), NodeComms(numaNode),
      parts(NumaAlloc<>(numaNode)),
      MarkerQueue(std::greater<>(), NumaAlloc(numaNode)),
      MarkerSet(std::less<>(), NumaAlloc(numaNode)) {}

int NodeCoordinator::GetNode() { return NumaNode; }

void NodeCoordinator::ProcessLocalResult(TaskResult &&result) {
#ifdef POC_DEBUG_POSITION
  assert(result.batchId <= result.windowId * 2);
  assert(result.endPos - result.startPos <= 9999);
#endif
  NodeComms.UpdateClock(result.batchId);
  auto destination = result.windowId % Setting::NODES_USED;
  NodeComms.SendJob(PassSegmentJob(std::move(result)), destination);
}

void NodeCoordinator::ProcessSegment(PassSegmentJob &segment) {
  std::lock_guard<std::mutex> guard(parts_mutex);
  auto &group = parts[segment.result.windowId];
  group.addResult(std::move(segment.result));
  MarkerQueue.push(
      ResultMarker{segment.result.windowId, segment.result.batchId + 1});
#ifdef POC_DEBUG
  std::stringstream stream;
  stream << "Stored results for windowId: " << segment.result.windowId
         << " total results: " << parts[segment.result.windowId].results.size()
         << std::endl;
  std::cout << stream.str();
#endif
}

void NodeCoordinator::MergeAndOutput(ResultGroup &group) {
  auto result = std::move(group.results[0]);
  for (size_t i = 1; i < group.results.size(); ++i) {
    MergeResults(result, group.results[i]);
  }
  OutputResult(result);
}

void NodeCoordinator::MergeResults(TaskResult &a, const TaskResult &b) {
  assert(std::abs(a.batchId - b.batchId) == 1);
  assert(a.windowId == b.windowId);
#ifdef POC_DEBUG
  std::stringstream stream;
  stream << "Merging windowId: " << a.windowId << " " << a.startPos << " - "
         << a.endPos << " (" << a.endPos - a.startPos << ")" << std::endl;
  std::cout << stream.str();
#endif
  YahooQuery::merge(a, b);
  a.batchId = std::max(a.batchId, b.batchId);
#ifdef POC_DEBUG_POSITION
  a.startPos = std::min(a.startPos, b.startPos);
  a.endPos = std::max(a.endPos, b.endPos);
#endif
}

void NodeCoordinator::ProcessJob(Job &job) {
  std::visit(
      overload{
          [this](PassSegmentJob &segment) { ProcessSegment(segment); },
          [this](MergeAndOutputJob &segment) { MergeAndOutput(segment.group); },
          [](int &) {}, [](QueryTask &) { throw std::exception(); }},
      job);
}

void NodeCoordinator::OutputResult(const TaskResult &result) {
#ifdef POC_DEBUG_POSITION
  // assert(result.endPos - result.startPos == 9999);
#endif
#ifdef POC_DEBUG
  std::stringstream stream;
  stream << "Outputting windowId: " << result.windowId << " " << result.startPos
         << " - " << result.endPos << " (" << result.endPos - result.startPos
         << ")" << std::endl;
  std::cout << stream.str();
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
  // Try to deque job
  auto job = NodeComms.TryGetJob();
  if (job) {

#ifdef POC_DEBUG
    std::stringstream stream;
    stream << "Node " << NumaNode << " Receiving job " << job.value()
           << std::endl;
    std::cout << stream.str();
#endif
    return std::move(job.value());
  }

  // Try to create merge+output job
  {
    std::unique_lock<std::mutex> lock(parts_mutex);
    if (lock.owns_lock()) {

      if (!MarkerQueue.empty() &&
          NodeComms.AllGreaterThan(MarkerQueue.top().batchId)) {
        auto iter = parts.find(MarkerQueue.top().windowId);
        MarkerQueue.pop();
        if (iter != parts.end()) {
          auto j = MergeAndOutputJob(std::move(iter->second));
          parts.erase(iter->first);
          return std::move(j);
        }
      }
    }
  }

  // Process next batch -> assuming it is going to be processed and throughput
  // can count it as such
  Setting::DataCounter.fetch_add(Setting::BATCH_SIZE,
                                 boost::memory_order_relaxed);
  auto batch = BatchCounter.fetch_add(1, boost::memory_order_relaxed);
  auto id = batch * Setting::NODES_USED + GetNode();
  return QueryTask{
      id,
      (char *)InputBuf + (batch % Setting::DATA_COUNT) * Setting::BATCH_SIZE,
      Setting::BATCH_COUNT,
      static_cast<long>((batch / Setting::DATA_COUNT) * Setting::NODES_USED *
                        Setting::DATA_COUNT * Setting::BATCH_COUNT / 10000)
#ifdef POC_DEBUG_POSITION
          ,
      id * Setting::BATCH_COUNT,
      (id + 1) * Setting::BATCH_COUNT - 1
#endif
  };
}

NodeComm &NodeCoordinator::GetComm() { return NodeComms; }
