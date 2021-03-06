#ifndef PROOFOFCONCEPT_NODECOORDINATOR_H
#define PROOFOFCONCEPT_NODECOORDINATOR_H

#include "AbstractNodeCoordinator.h"
#include "NumaAlloc.h"
#include "QueryTask.h"
#include "ResultGroup.h"
#include "Setting.h"
#include "SlidingWindowHandler.h"
#include "TaskResult.h"
#include <hash_fun.h>
#include <memory>
#include <queue>
#include <sstream>
#include <tbb/concurrent_priority_queue.h>
#include <tbb/concurrent_unordered_map.h>
#include <utility>
#include <vector>

template <template <typename> typename TQuery,
          template <template <typename> typename, typename>
          typename TTableProvider,
          template <typename TQ, typename NC> typename TResulMerger,
          template <typename NC> typename TWindowMarker>
class NodeCoordinator {
public:
  class Impl;

  using TableProvider = TTableProvider<TQuery, Impl>;
  using Query = TQuery<TableProvider>;
  using Slider = SlidingWindowHandler<Query>;
  using Marker = TWindowMarker<Impl>;
  using Merger = TResulMerger<Query, Impl>;

  class Impl : public AbstractNodeCoordinator<Query> {
  public:
    using RGroup = ResultGroup<typename Merger::ResultGroupData,
                               typename Marker::ResultGroupData>;
    static void InitializeSharedRegion() {
      auto mask = numa_allocate_nodemask();
      for (int i = 0; i < Setting::NODES_USED; ++i) {
        numa_bitmask_setbit(mask, i);
      }
      parts = reinterpret_cast<RGroup *>(malloc(sizeof(RGroup[Setting::SHARED_SLOTS])));
//      parts = reinterpret_cast<RGroup *>(numa_alloc_interleaved_subset(
//          sizeof(RGroup[Setting::SHARED_SLOTS]), mask));
      numa_free_nodemask(mask);
      new (parts) RGroup[Setting::SHARED_SLOTS];
      for (auto i = 0; i < Setting::SHARED_SLOTS; i++) {
        parts[i].windowId = -1;
      }
    }

  private:
    int NumaNode;
    void *InputBuf;
    std::atomic<int32_t> BatchCounter;
    std::vector<Impl *, NumaAlloc<Impl *>> coordinators;
    Marker marker;
    Merger merger;

    static RGroup *parts;

  public:
    static Slider slider;

    Impl(int numaNode, void *data)
        : NumaNode(numaNode), InputBuf(data), BatchCounter(0),
          coordinators(NumaAlloc<Impl *>(numaNode)), marker(*this),
          merger(*this) {}

    int GetNode() override { return NumaNode; }

    virtual void MarkGroupWithWindowId(RGroup &group, long windowId) {
      long snappedWindowId = group.windowId;
      if (snappedWindowId != windowId) {
        snappedWindowId = -1;
        while (
            !group.windowId.compare_exchange_weak(snappedWindowId, windowId) &&
            snappedWindowId != windowId) {
          snappedWindowId = -1;
          assert(snappedWindowId < windowId);
        }
      }
    }

    void ProcessLocalResult(typename Query::TResult &&result) override {
      merger.AddResults(std::move(result));
    }

    RGroup &GetResultGroup(long windowId) {
      return parts[windowId % Setting::SHARED_SLOTS];
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

      return QueryTask{batchId,
                       (char *)InputBuf +
                           (batch % Setting::DATA_COUNT) * Setting::BATCH_SIZE,
                       Setting::BATCH_COUNT,
                       Query::ComputeTimestampOffset(batch),
                       LatencyMonitor::GetTimestamp(),
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
      current = BatchCounter;
      lowest = current;
      // NodeCoordinator *slowest = nullptr;
      for (auto coordinator : coordinators) {
        int counter = coordinator->BatchCounter;
        if (lowest > counter) {
          lowest = counter;
          // slowest = coordinator;
        }
      }
      //        if (current - lowest > 50) {
      //          auto&& job = slowest->GetNextBatchTask();
      //          std::stringstream stream;
      //          stream << "Node " << GetNode()
      //                 << " stealing batch from: " << slowest->GetNode()
      //                 << " due to being behind by: " << current - lowest <<
      //                 " batchId: " << job.batchId << std::endl;
      //          std::cout << stream.str();
      //          return job;
      //        }
      if (current - lowest > 50) {
        return QueryTask{-1, nullptr, 0, 0, LatencyMonitor::Timestamp_t::max()};
      }
      while (marker.TryOutput())
        ;
      return GetNextBatchTask();
    }

    void SetCoordinators(std::vector<Impl *> coordinatorList) {
      this->coordinators.assign(coordinatorList.begin(), coordinatorList.end());
    }

    void Output(RGroup &group) {
#ifdef POC_DEBUG
      std::stringstream stream;
      char name[100];
      pthread_getname_np(pthread_self(), name, 100);
      stream << name << " Outputing windowId: " << group.windowId.load()
             << std::endl;
      std::cout << stream.str();
#endif
      auto &&map = merger.Output(group.mergerData);
      slider.OutputResult(std::move(map));
    }
  };
};

template <template <typename> typename TQuery,
          template <template <typename> typename, typename>
          typename TTableProvider,
          template <typename, typename> typename TResulMerger,
          template <typename> typename TWindowMarker>
typename NodeCoordinator<TQuery, TTableProvider, TResulMerger,
                         TWindowMarker>::Impl::RGroup
    *NodeCoordinator<TQuery, TTableProvider, TResulMerger,
                     TWindowMarker>::Impl::parts;

template <template <typename> typename TQuery,
          template <template <typename> typename, typename>
          typename TTableProvider,
          template <typename, typename> typename TResulMerger,
          template <typename> typename TWindowMarker>
typename NodeCoordinator<TQuery, TTableProvider, TResulMerger,
                         TWindowMarker>::Slider
    NodeCoordinator<TQuery, TTableProvider, TResulMerger,
                    TWindowMarker>::Impl::slider;
#endif // PROOFOFCONCEPT_NODECOORDINATOR_H
