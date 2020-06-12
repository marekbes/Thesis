#ifndef PROOFOFCONCEPT_THREADCOUNTWINDOWMARKER_H
#define PROOFOFCONCEPT_THREADCOUNTWINDOWMARKER_H

#include "Setting.h"
#include <atomic>
#include <mutex>

template <typename TCoordinator> class ThreadCountWindowMarker {
  TCoordinator &coordinator;
  static long lastOutputWindow;
  static std::mutex mutex;

public:
  struct ResultGroupData {
    std::atomic<long> threadSeenCounter;

    void reset() {
      long expected = Setting::THREADS_USED;
      assert(threadSeenCounter.compare_exchange_strong(expected, 0));
    }
  };

  explicit ThreadCountWindowMarker(TCoordinator &coordinator)
      : coordinator(coordinator) {}
  void MarkWindowDone(long fromWindowId, long toWindowId, int threadNumber);
  bool TryOutput() {
    auto &group = coordinator.GetResultGroup(lastOutputWindow);
    if (group.markerData.threadSeenCounter.load() != Setting::THREADS_USED) {
      return false;
    }
    std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if (lock.owns_lock()) {
      auto &group2 = coordinator.GetResultGroup(lastOutputWindow);
      if (group2.markerData.threadSeenCounter.load() == Setting::THREADS_USED) {
        coordinator.Output(group2);
        group2.reset();
        lastOutputWindow++;
        return true;
      }
    }
    return false;
  }
};

template <typename TCoordinator>
void ThreadCountWindowMarker<TCoordinator>::MarkWindowDone(long fromWindowId,
                                                           long toWindowId,
                                                           int) {
#ifdef POC_DEBUG
  std::stringstream stream;
  char name[100];
  pthread_getname_np(pthread_self(), name, 100);
  stream << name << " Incrementing from: " << fromWindowId
         << " to: " << toWindowId << std::endl;
  std::cout << stream.str();
#endif
  for (long windowId = fromWindowId; windowId < toWindowId; ++windowId) {
    auto &group = coordinator.GetResultGroup(windowId);
    coordinator.MarkGroupWithWindowId(group, windowId);
#ifdef POC_DEBUG
    auto preIncrement =
#endif
        group.markerData.threadSeenCounter.fetch_add(1);

#ifdef POC_DEBUG
    std::stringstream stream;
    char name[100];
    pthread_getname_np(pthread_self(), name, 100);
    stream << name << " Marking windowId: " << windowId
           << " new count: " << preIncrement + 1  << " seen id: " << group.windowId.load() << std::endl;
    std::cout << stream.str();
#endif
    //    if (preIncrement + 1 == Setting::THREADS_USED) {
    //      assert(group.windowId.compare_exchange_strong(windowId, -2));
    //      coordinator.Output(group);
    //#ifdef POC_DEBUG
    //      std::stringstream stream;
    //      stream << "Reset windowId: " << windowId
    //             << " new count: " << group.markerData.threadSeenCounter
    //             << std::endl;
    //      std::cout << stream.str();
    //#endif
    //    }
  }
}

template <typename TCoordinator>
long ThreadCountWindowMarker<TCoordinator>::lastOutputWindow;
template <typename TCoordinator>
std::mutex ThreadCountWindowMarker<TCoordinator>::mutex;

#endif // PROOFOFCONCEPT_THREADCOUNTWINDOWMARKER_H
