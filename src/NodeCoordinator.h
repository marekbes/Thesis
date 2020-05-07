#ifndef PROOFOFCONCEPT_NODECOORDINATOR_H
#define PROOFOFCONCEPT_NODECOORDINATOR_H

#include "NumaAlloc.h"
#include "QueryTask.h"
#include "ResultGroup.h"
#include "Setting.h"
#include "TaskResult.h"
#include "queries/YahooQuery.h"
#include <hash_fun.h>
#include <queue>
#include <sstream>
#include <tbb/concurrent_priority_queue.h>
#include <tbb/concurrent_unordered_map.h>
#include <utility>
#include <vector>

template <typename TQuery> class NodeCoordinator {
  using RGroup = ResultGroup<typename TQuery::KeyType, typename TQuery::GlobalValueType>;
  int NumaNode;
  volatile char outputArr[1024];
  volatile void *outputBuf = outputArr;
  void *InputBuf;
  boost::atomic_int BatchCounter;
  std::vector<NodeCoordinator *, NumaAlloc<NodeCoordinator *>> coordinators;

  static const int PARTS_COUNT = 10000;
  static RGroup parts[PARTS_COUNT];

public:
  NodeCoordinator(int numaNode, void *data)
      : NumaNode(numaNode), InputBuf(data), BatchCounter(0),
        coordinators(NumaAlloc<NodeCoordinator *>(numaNode)) {
    if (numaNode == 0) {

      for (auto &part : parts) {
        part.windowId = -1;
      }
    }
  }

  int GetNode() { return NumaNode; }

  void inline MarkGroupWithWindowId(RGroup &group, long windowId) {
    while (group.windowId != windowId) {
      assert(group.windowId <= windowId);
      long snappedWindowId = -1;
      if (group.windowId.compare_exchange_strong(snappedWindowId, windowId)) {
        break;
      }
    }
  }

  void
  ProcessLocalResult(typename TQuery::TResult &&result) {
#ifdef POC_DEBUG_POSITION
    assert(result.batchId <= result.windowId * 2);
    assert(result.endPos - result.startPos <= 9999);
#endif
    auto &group = parts[(result.windowId) % PARTS_COUNT];
    MarkGroupWithWindowId(group, result.windowId);
    auto table = result.result.get();
    auto buckets =
        table->getBuckets();
    for (size_t i = 0; i < table->getNumberOfBuckets(); ++i) {
      if (buckets[i].state) {
        auto &groupTable = group.results;
        while(true)
        {
          auto where = groupTable.find(buckets[i].key);
          std::pair<
              typename tbb::concurrent_unordered_map<typename TQuery::KeyType, std::atomic<int32_t>>::iterator,
              bool>
              where2;
          auto &value = where == groupTable.end() ? groupTable.emplace(buckets[i].key, 0).first->second : (*where).second;
          value += buckets[i].value._1;
          break;
        }
      }
    }
  }

  void markWindowDone(long windowId) {
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

    [[nodiscard]] QueryTask GetNextBatchTask() {

        // Process next batch
        auto batch = BatchCounter.fetch_add(1, boost::memory_order_relaxed);
        auto batchId = batch * Setting::NODES_USED + GetNode();

        // Assuming it is going to be processed and throughput can count it as such
        Setting::DataCounter.fetch_add(Setting::BATCH_SIZE,
                                       boost::memory_order_relaxed);

        return QueryTask{
                batchId,
                (char *)InputBuf + (batch % Setting::DATA_COUNT) * Setting::BATCH_SIZE,
                Setting::BATCH_COUNT,TQuery::ComputeTimestampOffset(batch)
#ifdef POC_DEBUG_POSITION
        ,
        batchId * Setting::BATCH_COUNT,
        (batchId + 1) * Setting::BATCH_COUNT - 1
#endif
      };
  }

  [[nodiscard]] QueryTask GetJob() {

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
      return slowest->GetNextBatchTask();
    }
    return GetNextBatchTask();
  }

  void SetCoordinators(std::vector<NodeCoordinator *> coordinatorList) {
    this->coordinators.assign(coordinatorList.begin(), coordinatorList.end());
  }
};

template <typename TQuery>
typename NodeCoordinator<TQuery>::RGroup NodeCoordinator<TQuery>::parts[PARTS_COUNT];
#endif // PROOFOFCONCEPT_NODECOORDINATOR_H
