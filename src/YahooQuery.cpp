#include "YahooQuery.h"

#include "Setting.h"
#include <utility>

YahooQuery::YahooQuery(const std::vector<long> &staticJoinData)
    : StaticJoinTable(2048) {
  for (size_t i = 0; i < staticJoinData.size() / 2; ++i) {
    this->StaticJoinTable.insert(staticJoinData[i * 2],
                                 staticJoinData[i * 2 + 1], 0);
  }
}

void YahooQuery::process(const QueryTask &task) {
  bool startingWindow = false;
  auto countMap = TablePool.acquire();
  auto input = reinterpret_cast<const InputSchema *>(task.data);
  auto lastWindowId = input[0].timestamp / 10000;
  for (size_t i = 0; i < task.size; ++i) {
    auto windowId = input[i].timestamp / 10000;
    if (windowId != lastWindowId) {
      this->OutputCb(TaskResult{lastWindowId, startingWindow, true, 0,
                                std::move(countMap), task.batchId});
      startingWindow = true;
      countMap = this->TablePool.acquire();
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
    countMap->insert_or_modify(campaignId, curVal, 0);
  }
  this->OutputCb(TaskResult{lastWindowId, startingWindow, false, 0,
                            std::move(countMap), task.batchId});
}

void YahooQuery::merge(TaskResult &a, const TaskResult &b) {
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

void YahooQuery::SetOutputCb(std::function<void(TaskResult)> outputCb) {
  this->OutputCb = std::move(outputCb);
}
