#ifndef PROOFOFCONCEPT_YAHOOQUERY_H
#define PROOFOFCONCEPT_YAHOOQUERY_H

#include "../QueryTask.h"
#include "../Setting.h"
#include <atomic>
#include <fstream>
#include <functional>
#include <memory>
#include <numa.h>
#include <sstream>
#include <utility>

struct CounterVal {
  long _1;
};

template <typename TMapProvider> class YahooQuery {
private:
  using MapProvider = TMapProvider;

public:
  using KeyType = long;
  using LocalValueType = CounterVal;
  using GlobalValueType = std::atomic<int64_t>;
  using TResult = typename MapProvider::TResult;
  using MapType = typename MapProvider::MapType;
  struct InputSchema {
    long timestamp;
    long padding_0;
    long user_id;
    long page_id;
    long ad_id;
    long ad_type;
    long event_type;
    long ip_address;
  };

  HashTable<long, long> StaticJoinTable;
  MapProvider TablePool;
  std::function<void(TResult &&)> OutputCb;

  inline static long ComputeTimestamp (const InputSchema &input,
                                      const long timestampOffset) {
    return (input.timestamp + timestampOffset) / Setting::WINDOW_SLIDE;
  }

public:
  YahooQuery(std::vector<char> &staticJoinData, MapProvider provider)
      : StaticJoinTable(2048), TablePool(provider) {
    auto data = reinterpret_cast<long *>(staticJoinData.data());
    for (size_t i = 0; i < staticJoinData.size() / 2 / sizeof(long); ++i) {
      this->StaticJoinTable.insert(data[i * 2], data[i * 2 + 1], 0);
    }
  }

  void process(const QueryTask &task) {
    bool startingWindow = false;

    auto input = reinterpret_cast<const InputSchema *>(task.data);
    auto lastWindowId = ComputeTimestamp(input[0], task.timestampOffset);
    auto countMap = TablePool.acquire(lastWindowId);
#ifdef POC_DEBUG_POSITION
    auto lastOutputPos = task.startPos;
#endif
    for (int i = 0; i < task.size; ++i) {
      auto windowId = ComputeTimestamp(input[i], task.timestampOffset);
      if (windowId != lastWindowId) {
#ifdef POC_DEBUG_POSITION
        this->OutputCb(TResult{lastWindowId, startingWindow, true, 0,
                               std::move(countMap), task.batchId,
                               task.latencyMark, lastOutputPos,
                               task.startPos + i - 1});
        lastOutputPos = task.startPos + i;
#else
        OutputCb(TResult{lastWindowId, startingWindow, true, 0,
                         std::move(countMap), task.batchId, task.latencyMark});
#endif
        startingWindow = true;
        countMap = TablePool.acquire(windowId);
        lastWindowId = windowId;
      }
      if (input[i].event_type != 0) {
        continue;
      }
      long campaignId = 0;
      auto foundId = StaticJoinTable.get_value(input[i].ad_id, campaignId);
      if (!foundId) {
        continue;
      }
      CounterVal curVal{1};
      countMap->insert_or_modify(campaignId, curVal, windowId);
    }
    OutputCb(TResult{lastWindowId, startingWindow, false, 0,
                     std::move(countMap), task.batchId, task.latencyMark
#ifdef POC_DEBUG_POSITION
                     ,
                     lastOutputPos, task.endPos
#endif
    });
  }

  static void merge(TResult &a, const TResult &b) {
    Bucket<long, CounterVal> *buckets = b.result->getBuckets();
    for (size_t i = 0; i < b.result->getNumberOfBuckets(); ++i) {
      if (buckets[i].state) {
        a.result->insert_or_modify(buckets[i].key, buckets[i].value,
                                   buckets[i].timestamp);
      }
    }
    a.startingWindow |= b.startingWindow;
    a.endingWindow |= b.endingWindow;
  }

  void SetOutputCb(std::function<void(TResult &&)> outputCb) {
    this->OutputCb = std::move(outputCb);
  }

  static std::vector<char> loadStaticData(int totalNodes) {
    auto path =
        Setting::DATA_PATH + "input_" + std::to_string(totalNodes) + ".dat";
    std::ifstream file(path, std::ifstream::binary);
    if (file.fail()) {
      throw std::invalid_argument("missing input data: " + path);
    }
    unsigned int len = 0;
    file.read((char *)&len, sizeof(len));
    std::vector<char> data;
    data.resize(len * sizeof(long));
    if (len > 0)
      file.read(data.data(), len * sizeof(long));
    return data;
  }

  static void *loadData(int node, int totalNodes) {
    auto path = Setting::DATA_PATH + "input_" + std::to_string(node) + "_" +
                std::to_string(totalNodes) + ".dat";
    std::ifstream file(path, std::ifstream::ate | std::ifstream::binary);
    if (file.fail()) {
      throw std::invalid_argument("missing input data: " + path);
    }
    auto size = file.tellg();
    file.seekg(0);
    Setting::DATA_COUNT = size / Setting::BATCH_SIZE;
    assert(size % Setting::BATCH_SIZE == 0);
    auto data = numa_alloc_onnode(size, node);
    file.read(static_cast<char *>(data), size);
    return data;
  }

  static long ComputeTimestampOffset(int batch) {
    return (batch / Setting::DATA_COUNT) * Setting::DATA_COUNT *
           Setting::BATCH_COUNT * Setting::NODES_USED;
  }

  static inline LocalValueType ConvertToLocal(GlobalValueType &global) {
    return LocalValueType{global.load()};
  }
};

#endif // PROOFOFCONCEPT_YAHOOQUERY_H
