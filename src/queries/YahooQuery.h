#ifndef PROOFOFCONCEPT_YAHOOQUERY_H
#define PROOFOFCONCEPT_YAHOOQUERY_H

#include "../HashTable.h"
#include "../HashTablePool.h"
#include "../QueryTask.h"
#include "../TaskResult.h"
#include <functional>
#include <memory>
#include <atomic>

class YahooQuery {
public:
  using KeyType = long;
  struct CounterVal {
    long _1;
    long _2;
  };
  using LocalValueType = CounterVal;
  using GlobalValueType = std::atomic<int64_t>;
  using TResult = TaskResult<KeyType, LocalValueType>;
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
  HashTablePool<long, CounterVal> TablePool;
  std::function<void(TResult &&)> OutputCb;
  inline static long ComputeTimestamp(const InputSchema& input, const long timestampOffset);

public:
  explicit YahooQuery(std::vector<char> &staticJoinData);
  void process(const QueryTask &task);
  void SetOutputCb(std::function<void(TResult &&)> outputCb);
  static void merge(TResult &a, const TResult &b);
  static void* loadData(int node, int totalNodes);
  static std::vector<char> loadStaticData(int totalNodes);
  static long ComputeTimestampOffset(int batch);
};

#endif // PROOFOFCONCEPT_YAHOOQUERY_H
