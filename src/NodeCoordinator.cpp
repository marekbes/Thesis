#include "NodeCoordinator.h"
#include "Setting.h"
#include "YahooQuery.h"
#include <sstream>
#include <utility>

template <class... Ts> struct overload : Ts... { using Ts::operator()...; };
template <class... Ts> overload(Ts...)->overload<Ts...>;

NodeCoordinator::NodeCoordinator(int numaNode, void *data)
    : NumaNode(numaNode), InputBuf(data), BatchCounter(0),
      parts(PARTS_COUNT, NumaAlloc<>(numaNode)),
      MarkerQueue(NumaAlloc(numaNode)),
      MarkerSet(std::less<>(), NumaAlloc(numaNode)), NodeComms(numaNode, *this) {
  for (int i = 0; i < PARTS_COUNT; ++i) {
    parts[i].windowId = -1;
  }
}

int NodeCoordinator::GetNode() { return NumaNode; }

void NodeCoordinator::ProcessLocalResult(TaskResult &&result) {
#ifdef POC_DEBUG_POSITION
  assert(result.batchId <= result.windowId * 2);
  assert(result.endPos - result.startPos <= 9999);
#endif
  auto destination = result.windowId % Setting::NODES_USED;
  NodeComms.SendJob(PassSegmentJob(std::move(result)), destination);
}

void NodeCoordinator::ProcessSegment(PassSegmentJob &segment) {
  auto &group =
      parts[(segment.result.windowId / Setting::NODES_USED) % PARTS_COUNT];
  auto windowId = segment.result.windowId;
  long snappedWindowId = group.windowId;
  if (snappedWindowId != windowId) {
    snappedWindowId = -1;
    assert(group.windowId.compare_exchange_weak(snappedWindowId, windowId));
  }
  auto position = group.resultCount.fetch_add(1);
  assert(position < ResultGroup::RESULT_COUNT_LIMIT);
  group.results[position] = std::move(segment.result);
  MarkerQueue.push(
      ResultMarker{segment.result.windowId, segment.result.batchId});
#ifdef POC_DEBUG
  std::stringstream stream;
  stream << "Stored results for windowId: " << segment.result.windowId
         << " total results: " << position + 1 << std::endl;
  std::cout << stream.str();
#endif
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

void NodeCoordinator::ProcessJob(Job &job) {
  std::visit(
      overload{[this](PassSegmentJob &segment) { ProcessSegment(segment); },
               [this](MergeAndOutputJob &segment) {
                 MergeAndOutput(segment.resultCount, segment.results);
               },
               [this](MarkBatchComplete &markBatch) {
                 NodeComms.UpdateClock(markBatch.batchId);
               },
               [](int &) { throw std::exception(); },
               [](QueryTask &) { throw std::exception(); }},
      job);
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
    ResultMarker marker{};
    if (MarkerQueue.try_pop(marker)) {
#ifdef POC_DEBUG
      std::stringstream stream;
      stream << "Node " << NumaNode << " popped batchId: " << marker.batchId
             << " windowId: " << marker.windowId << std::endl;
      std::cout << stream.str();
#endif
      if (NodeComms.AllGreaterThan(marker.batchId + 100)) {
        auto windowId = marker.windowId;
        auto &group = parts[(windowId / Setting::NODES_USED) % PARTS_COUNT];
        if (group.windowId.compare_exchange_weak(windowId, -2)) {
          auto j = MergeAndOutputJob(group.resultCount, group.results);
#ifdef POC_DEBUG
          for (int i = 0; i < group.resultCount; ++i) {
            assert(group.results[i].result.get() == nullptr);
          }
#endif
          group.resultCount = 0;
          group.windowId = -1;
          return std::move(j);
        } else {
#ifdef POC_DEBUG
          std::stringstream stream;
          stream << "Node " << NumaNode
                 << " discarding batchId: " << marker.batchId
                 << " windowId: " << marker.windowId << " new value "
                 << windowId << std::endl;
          std::cout << stream.str();
#endif
        }
      } else {
#ifdef POC_DEBUG
        std::stringstream stream;
        stream << "Node " << NumaNode
               << " returning batchId: " << marker.batchId
               << " windowId: " << marker.windowId << std::endl;
        std::cout << stream.str();
#endif
        MarkerQueue.push(marker);
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
      static_cast<long>((batch / Setting::DATA_COUNT) * Setting::DATA_COUNT *
                        Setting::BATCH_COUNT * Setting::NODES_USED)
#ifdef POC_DEBUG_POSITION
          ,
      id * Setting::BATCH_COUNT,
      (id + 1) * Setting::BATCH_COUNT - 1
#endif
  };
}
