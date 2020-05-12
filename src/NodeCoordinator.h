#ifndef PROOFOFCONCEPT_NODECOORDINATOR_H
#define PROOFOFCONCEPT_NODECOORDINATOR_H

#include "AbstractNodeCoordinator.h"
#include "NumaAlloc.h"
#include "QueryTask.h"
#include "ResultGroup.h"
#include "Setting.h"
#include "TaskResult.h"
#include <hash_fun.h>
#include <queue>
#include <sstream>
#include <tbb/concurrent_priority_queue.h>
#include <tbb/concurrent_unordered_map.h>
#include <utility>
#include <vector>

template <typename TQuery,
          template <typename TQ, typename NC> typename TResultMerger,
          template <typename NC> typename TWindowMarker>
class NodeCoordinator : public AbstractNodeCoordinator<TQuery> {
  using TMarker = TWindowMarker<NodeCoordinator>;
  using TMerger = TResultMerger<TQuery, NodeCoordinator>;
  using RGroup = ResultGroup<typename TMerger::ResultGroupData,
                             typename TMarker::ResultGroupData>;
  int NumaNode;
  void *InputBuf;
  std::atomic<int32_t> BatchCounter;
  std::vector<NodeCoordinator *, NumaAlloc<NodeCoordinator *>> coordinators;
  TMarker marker;
  TMerger merger;

  static const int PARTS_COUNT = 5000;
  static RGroup parts[PARTS_COUNT];

public:
  NodeCoordinator(int numaNode, void *data)
      : NumaNode(numaNode), InputBuf(data), BatchCounter(0),
        coordinators(NumaAlloc<NodeCoordinator *>(numaNode)), marker(*this),
        merger(*this) {
    if (numaNode == 0) {
      for (auto &part : parts) {
        part.windowId = -1;
      }
    }
  }

  int GetNode() override { return NumaNode; }

  virtual void MarkGroupWithWindowId(RGroup &group, long windowId) {
    long snappedWindowId = group.windowId;
    if (snappedWindowId != windowId) {
      snappedWindowId = -1;
      while (!group.windowId.compare_exchange_weak(snappedWindowId, windowId) &&
             snappedWindowId != windowId) {
        snappedWindowId = -1;
        assert(snappedWindowId < windowId);
      }
    }
  }

  void ProcessLocalResult(typename TQuery::TResult &&result) override {
    merger.AddResults(std::move(result));
  }

  RGroup &GetResultGroup(long windowId) {
    return parts[windowId % PARTS_COUNT];
  }

  void MarkWindowDone(long fromWindowId, long toWindowId,
                      int threadNumber) override {
    marker.MarkWindowDone(fromWindowId, toWindowId, threadNumber);
  }

  [[nodiscard]] QueryTask GetNextBatchTask() {

    // Process next batch
    auto batch = BatchCounter.fetch_add(1);
    auto batchId = batch * Setting::NODES_USED + GetNode();

    // Assuming it is going to be processed and throughput can count it as
    // such
    Setting::DataCounter.fetch_add(Setting::BATCH_SIZE);

    return QueryTask{
        batchId,
        (char *)InputBuf + (batch % Setting::DATA_COUNT) * Setting::BATCH_SIZE,
        Setting::BATCH_COUNT,
        TQuery::ComputeTimestampOffset(batch)
#ifdef POC_DEBUG_POSITION
            ,
        batchId * Setting::BATCH_COUNT,
        (batchId + 1) * Setting::BATCH_COUNT - 1
#endif
    };
  }

  [[nodiscard]] QueryTask GetJob() override {

    // Process next batch

    // Wait until not too far ahead
    int current = BatchCounter;
    int lowest = current;
    do {
      current = BatchCounter;
      lowest = current;
      //NodeCoordinator *slowest = nullptr;
      for (auto coordinator : coordinators) {
        int counter = coordinator->BatchCounter;
        if (lowest > counter) {
          lowest = counter;
          //slowest = coordinator;
        }
      }
      //        if (current - lowest > 50) {
      //          auto&& job = slowest->GetNextBatchTask();
      //          std::stringstream stream;
      //          stream << "Node " << GetNode()
      //                 << " stealing batch from: " << slowest->GetNode()
      //                 << " due to being behind by: " << current - lowest << "
      //                 batchId: " << job.batchId << std::endl;
      //          std::cout << stream.str();
      //          return job;
      //        }
    } while (current - lowest > 50);
    while (marker.TryOutput());
    return GetNextBatchTask();
  }

  void SetCoordinators(std::vector<NodeCoordinator *> coordinatorList) {
    this->coordinators.assign(coordinatorList.begin(), coordinatorList.end());
  }

  void Output(RGroup &group) {
    merger.Output(group.mergerData);
    group.reset();
  }
};

template <typename TQuery,
          template <typename TQ, typename NC> typename TResultMerger,
          template <typename NC> typename TWindowMarker>
typename NodeCoordinator<TQuery, TResultMerger, TWindowMarker>::RGroup
    NodeCoordinator<TQuery, TResultMerger, TWindowMarker>::parts[PARTS_COUNT];
#endif // PROOFOFCONCEPT_NODECOORDINATOR_H
