#ifndef PROOFOFCONCEPT_THREADCLOCKWINDOWMARKER_H
#define PROOFOFCONCEPT_THREADCLOCKWINDOWMARKER_H

#include "Setting.h"
#include <atomic>
#include <mutex>

template <typename TCoordinator> class ThreadClockWindowMarker {
  TCoordinator &coordinator;
  static const int MAX_THREADS = 32;
  static std::atomic<long> counters[MAX_THREADS];
  static std::atomic<long> maxAgreedWindow;
  static long lastOutputWindow;
  static std::mutex mutex;

public:
  struct ResultGroupData {
    void reset() {}
  };

  ThreadClockWindowMarker(TCoordinator &coordinator)
      : coordinator(coordinator) {}
  void MarkWindowDone(long fromWindowId, long toWindowId, int threadNumber);
  bool TryOutput() {
    if (maxAgreedWindow.load() > lastOutputWindow) {
      std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
      if (lock.owns_lock()) {
        coordinator.Output(coordinator.GetResultGroup(lastOutputWindow));
        lastOutputWindow++;
        return true;
      }
    }
    return false;
  }
};

template <typename TCoordinator>
void ThreadClockWindowMarker<TCoordinator>::MarkWindowDone(long,
                                                           long toWindowId,
                                                           int threadNumber) {
#ifdef POC_DEBUG
  {
    std::stringstream stream;
    char name[100];
    pthread_getname_np(pthread_self(), name, 100);
    stream << name << " Incrementing from: " << fromWindowId
           << " to: " << toWindowId << std::endl;
    std::cout << stream.str();
  }
#endif
  long snapCounter = counters[threadNumber];
  assert(snapCounter <= toWindowId);
  if (snapCounter < toWindowId) {
    counters[threadNumber] = toWindowId;
    long snapMax = maxAgreedWindow;
    long min = toWindowId;
    for (int i = 0; i < Setting::THREADS_USED; ++i) {
      min = std::min(counters[i].load(), min);
    }
#ifdef POC_DEBUG
    {
      std::stringstream stream;
      char name[100];
      pthread_getname_np(pthread_self(), name, 100);
      stream << name << " Min from: " << min << " maxSnap: " << snapMax
             << std::endl;
      std::cout << stream.str();
    }
#endif
    while (min > snapMax) {
      maxAgreedWindow.compare_exchange_weak(snapMax, min);
    }
  }
}

template <typename TCoordinator>
std::atomic<long> ThreadClockWindowMarker<TCoordinator>::counters[MAX_THREADS];
template <typename TCoordinator>
std::atomic<long> ThreadClockWindowMarker<TCoordinator>::maxAgreedWindow;
template <typename TCoordinator>
long ThreadClockWindowMarker<TCoordinator>::lastOutputWindow;
template <typename TCoordinator>
std::mutex ThreadClockWindowMarker<TCoordinator>::mutex;
#endif // PROOFOFCONCEPT_THREADCLOCKWINDOWMARKER_H
