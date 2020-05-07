#include "YahooQuery.h"

#include "../Setting.h"
#include <fstream>
#include <numa.h>
#include <sstream>
#include <utility>

YahooQuery::YahooQuery(std::vector<char> &staticJoinData)
    : StaticJoinTable(2048) {
  auto data = reinterpret_cast<long *>(staticJoinData.data());
  for (size_t i = 0; i < staticJoinData.size() / 2 / sizeof(long); ++i) {
    this->StaticJoinTable.insert(data[i * 2], data[i * 2 + 1], 0);
  }
}

void YahooQuery::process(const QueryTask &task) {
  bool startingWindow = false;
  auto countMap = TablePool.acquire();

  auto input = reinterpret_cast<const InputSchema *>(task.data);
  auto lastWindowId = (input[0].timestamp + task.timestampOffset) / 10000;
#ifdef POC_DEBUG_POSITION
  auto lastOutputPos = task.startPos;
#endif
  for (int i = 0; i < task.size; ++i) {
    auto windowId = (input[i].timestamp + task.timestampOffset) / 10000;
    if (windowId != lastWindowId) {
#ifdef POC_DEBUG_POSITION
      this->OutputCb(TResult{lastWindowId, startingWindow, true, 0,
                             std::move(countMap), task.batchId, lastOutputPos,
                             task.startPos + i - 1});
      lastOutputPos = task.startPos + i;
#else
      this->OutputCb(TResult{lastWindowId, startingWindow, true, 0,
                             std::move(countMap), task.batchId});
#endif
      startingWindow = true;
      countMap = this->TablePool.acquire();
#ifdef POC_DEBUG
      std::stringstream stream;
      stream << "Map " << countMap.get() << "for batch " << std::dec
             << task.batchId << std::endl;
      std::cout << stream.str();
#endif
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
    CounterVal curVal{1, input[i].timestamp};
    countMap->insert_or_modify(campaignId, curVal, windowId);
  }
  this->OutputCb(TResult{lastWindowId, startingWindow, false, 0,
                         std::move(countMap), task.batchId
#ifdef POC_DEBUG_POSITION
                         ,
                         lastOutputPos, task.endPos
#endif
  });
}

void YahooQuery::merge(TResult &a, const TResult &b) {
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

void YahooQuery::SetOutputCb(std::function<void(TResult &&)> outputCb) {
  this->OutputCb = std::move(outputCb);
}
std::vector<char> YahooQuery::loadStaticData(int totalNodes) {
  std::ifstream file(Setting::DATA_PATH + "input_" +
                         std::to_string(totalNodes) + ".dat",
                     std::ifstream::binary);
  if (file.fail()) {
    throw std::invalid_argument("missing input data");
  }
  unsigned int len = 0;
  file.read((char *)&len, sizeof(len));
  std::vector<char> data;
  data.resize(len * sizeof(long));
  if (len > 0)
    file.read(data.data(), len * sizeof(long));
  return data;
}

void *YahooQuery::loadData(int node, int totalNodes) {
  auto path = Setting::DATA_PATH + "input_" + std::to_string(node) +
              "_" + std::to_string(totalNodes) + ".dat";
  std::ifstream file(,
                     std::ifstream::ate | std::ifstream::binary);
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

long YahooQuery::ComputeTimestampOffset(int batch) {
  return (batch / Setting::DATA_COUNT) * Setting::DATA_COUNT *
         Setting::BATCH_COUNT * Setting::NODES_USED;
}
