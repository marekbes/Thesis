#ifndef PROOFOFCONCEPT_YAHOOQUERY_H
#define PROOFOFCONCEPT_YAHOOQUERY_H

#include "HashTable.h"
#include "HashTablePool.h"
#include "QueryTask.h"
#include "TaskResult.h"
#include <functional>
#include <memory>

class YahooQuery {

  HashTable<long, long> StaticJoinTable;
  HashTablePool<long, CounterVal> TablePool;
  std::function<void(TaskResult)> OutputCb;

public:
  explicit YahooQuery(const std::vector<long> &staticJoinData);
  void process(const QueryTask &task);
  void SetOutputCb(std::function<void(TaskResult&&)> outputCb);
  static void merge(TaskResult &a, const TaskResult &b);
};

#endif // PROOFOFCONCEPT_YAHOOQUERY_H
