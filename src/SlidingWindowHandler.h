#ifndef THESIS_SLIDINGWINDOWHANDLER_H
#define THESIS_SLIDINGWINDOWHANDLER_H
#include "HashTablePool.h"
#include "Setting.h"

template <class Query> class SlidingWindowHandler {
public:
  struct SliderResult {
  public:
    typename Query::MapType map;
    LatencyMonitor::Timestamp_t timestamp;
  };

private:
  int slideSlots;
  std::vector<SliderResult> results;
  typename Query::MapType::element_type tempResult;
  int currentResult = 0;

public:
  void Init()
  {
    slideSlots = Setting::WINDOW_SIZE / Setting::WINDOW_SLIDE;
    results.resize(slideSlots);
  }

  void OutputResult(SliderResult &&r) {
    LatencyMonitor::InsertMeasurement(r.timestamp);
    if (Setting::WINDOW_SLIDE < Setting::WINDOW_SIZE) {
      tempResult.merge(*r.map);
      if (currentResult >= Setting::WINDOW_SIZE / Setting::WINDOW_SLIDE) {
        auto &subtract = results[currentResult % slideSlots].map;
        auto *buckets = subtract->getBuckets();
        for (size_t i = 0; i < subtract->getNumberOfBuckets(); ++i) {
          if (buckets[i].state) {
            tempResult.insert_or_modify(buckets[i].key, buckets[i].value,
                                        buckets[i].timestamp);
          }
        }
      }
      // Output here
      results[currentResult % slideSlots] = std::move(r);
      currentResult++;
    }
  }
};

#endif // THESIS_SLIDINGWINDOWHANDLER_H
