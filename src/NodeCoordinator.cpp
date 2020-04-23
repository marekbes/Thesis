#include "NodeCoordinator.h"
#include "Setting.h"
#include "YahooQuery.h"
#include <sstream>
#include <utility>

template <class... Ts> struct overload : Ts... { using Ts::operator()...; };
template <class... Ts> overload(Ts...)->overload<Ts...>;
ResultGroup NodeCoordinator::parts[PARTS_COUNT];

NodeCoordinator::NodeCoordinator(int numaNode, void *data)
    : NumaNode(numaNode), InputBuf(data), BatchCounter(0),
      coordinators(NumaAlloc<NodeCoordinator *>(numaNode)),
      NodeComms(numaNode, *this) {
  for (auto &part : parts) {
    part.windowId = -1;
  }
}

int NodeCoordinator::GetNode() { return NumaNode; }

void inline MarkGroupWithWindowId(ResultGroup &group, long windowId) {
  while (group.windowId != windowId) {
    assert(group.windowId <= windowId);
    long snappedWindowId = -1;
    if(group.windowId.compare_exchange_strong(snappedWindowId, windowId))
    {
      break;
    }
  }
}

void NodeCoordinator::ProcessLocalResult(TaskResult &&result) {
#ifdef POC_DEBUG_POSITION
  assert(result.batchId <= result.windowId * 2);
  assert(result.endPos - result.startPos <= 9999);
#endif
  auto &group = parts[(result.windowId) % PARTS_COUNT];
  MarkGroupWithWindowId(group, result.windowId);
  auto position = group.resultCount.fetch_add(1);
  assert(position < ResultGroup::RESULT_COUNT_LIMIT);
  group.results[position] = std::move(result);
}

void NodeCoordinator::markWindowDone(long windowId) {
  auto &group = parts[windowId % PARTS_COUNT];
  MarkGroupWithWindowId(group, windowId);
  auto preIncrement = group.threadSeenCounter.fetch_add(1);
#ifdef POC_DEBUG
  std::stringstream stream;
  stream << "Marking windowId: " << windowId
         << " new count: " << preIncrement + 1 << std::endl;
  std::cout << stream.str();
#endif
  if (preIncrement + 1 == Setting::THREADS_USED) {
    assert(group.windowId.compare_exchange_strong(windowId, -2));
    this->MergeAndOutput(group.resultCount, group.results);
    group.reset();
#ifdef POC_DEBUG
    std::stringstream stream;
  stream << "Reset windowId: " << windowId
         << " new count: " << group.threadSeenCounter << std::endl;
  std::cout << stream.str();
#endif
  }
}

void NodeCoordinator::MergeAndOutput(
    size_t resultCount, TaskResult results[ResultGroup::RESULT_COUNT_LIMIT]) {
  for (size_t i = 1; i < resultCount; ++i) {
    MergeResults(results[0], results[i]);
  }
  OutputResult(results[0]);
}

void NodeCoordinator::MergeResults(TaskResult &a, const TaskResult &b) {
  assert(std::abs(a.batchId - b.batchId) == 1);
  assert(a.windowId == b.windowId);
#ifdef POC_DEBUG
  std::stringstream stream;
  stream << "Merging windowId: " << a.windowId << " "
#ifdef POC_DEBUG_POSITION
         << a.startPos << " - " << a.endPos << " (" << a.endPos - a.startPos
         << ")"
#endif
         << std::endl;
  std::cout << stream.str();
#endif
  YahooQuery::merge(a, b);
  a.batchId = std::max(a.batchId, b.batchId);
#ifdef POC_DEBUG_POSITION
  a.startPos = std::min(a.startPos, b.startPos);
  a.endPos = std::max(a.endPos, b.endPos);
#endif
}

void NodeCoordinator::OutputResult(const TaskResult &result) {
#ifdef POC_DEBUG_POSITION
  assert(result.endPos - result.startPos == 9999);
#endif
#ifdef POC_DEBUG
  std::stringstream stream;
  stream << "Outputting windowId: " << result.windowId << " "
#ifdef POC_DEBUG_POSITION
         << result.startPos << " - " << result.endPos << " ("
         << result.endPos - result.startPos << ")"
#endif
         << std::endl;
  std::cout << stream.str();
#endif
//  auto &table = result.result;
//  for (size_t j = 0; j < table->getNumberOfBuckets(); ++j) {
//    auto bucket = table->getBuckets()[j];
//    if (bucket.state) {
//      *(long *)outputBuf = bucket.key;
//      *(long *)outputBuf = bucket.value._1;
//    }
//  }
}

[[nodiscard]] QueryTask NodeCoordinator::GetJob() {

  // Process next batch
  auto batch = BatchCounter.fetch_add(1, boost::memory_order_relaxed);
  auto batchId = batch * Setting::NODES_USED + GetNode();

  // Wait until not too far ahead
  while (true) {
    int lowest = batch;
    for (auto &coordinator : coordinators) {
      lowest = std::min(lowest, static_cast<int>(coordinator->BatchCounter));
    }
    if (batch - lowest < 10) {
      break;
    }
  }

  // Assuming it is going to be processed and throughput can count it as such
  Setting::DataCounter.fetch_add(Setting::BATCH_SIZE,
                                 boost::memory_order_relaxed);

  return QueryTask{
      batchId,
      (char *)InputBuf + (batch % Setting::DATA_COUNT) * Setting::BATCH_SIZE,
      Setting::BATCH_COUNT,
      static_cast<long>((batch / Setting::DATA_COUNT) * Setting::DATA_COUNT *
                        Setting::BATCH_COUNT * Setting::NODES_USED)
#ifdef POC_DEBUG_POSITION
          ,
      batchId * Setting::BATCH_COUNT,
      (batchId + 1) * Setting::BATCH_COUNT - 1
#endif
  };
}
void NodeCoordinator::SetCoordinators(
    std::vector<NodeCoordinator *> coordinatorList) {
  this->coordinators.assign(coordinatorList.begin(), coordinatorList.end());
}
