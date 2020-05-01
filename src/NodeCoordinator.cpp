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
    if(numaNode == 0)
    {

        for (auto &part : parts) {
            part.windowId = -1;
        }
    }
}

int NodeCoordinator::GetNode() { return NumaNode; }

void inline MarkGroupWithWindowId(ResultGroup &group, long windowId) {
  while (group.windowId != windowId) {
    assert(group.windowId <= windowId);
    long snappedWindowId = -1;
    if (group.windowId.compare_exchange_strong(snappedWindowId, windowId)) {
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
  auto table = result.result.get();
  Bucket<long, CounterVal> *buckets = table->getBuckets();
  for (size_t i = 0; i < table->getNumberOfBuckets(); ++i) {
    if (buckets[i].state) {
        group.results[buckets[i].key] += buckets[i].value._1;
    }
  }
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
    // OUTPUT DUMMY ...
    group.reset();
#ifdef POC_DEBUG
    std::stringstream stream;
    stream << "Reset windowId: " << windowId
           << " new count: " << group.threadSeenCounter << std::endl;
    std::cout << stream.str();
#endif
  }
}


[[nodiscard]] QueryTask NodeCoordinator::GetJob() {

  // Process next batch
  int current = BatchCounter;

  // Wait until not too far ahead
  int lowest = current;
  NodeCoordinator *slowest = nullptr;
  for (auto coordinator : coordinators) {
    lowest = std::min(lowest, static_cast<int>(coordinator->BatchCounter));
    slowest = coordinator;
  }
  if (current - lowest > 100) {
    return slowest->GetJob();
  }

  // Process next batch
  auto batch = BatchCounter.fetch_add(1, boost::memory_order_relaxed);
  auto batchId = batch * Setting::NODES_USED + GetNode();

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
