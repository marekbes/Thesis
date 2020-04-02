#include "ResultGroup.h"

void ResultGroup::addResult(TaskResult &result) {
  results.insert(std::upper_bound(results.begin(), results.end(), result,
                                  [](const TaskResult &a, const TaskResult &b) {
                                    return a.batchId < b.batchId;
                                  }), std::move(result));
}
bool ResultGroup::isComplete() {
  return std::adjacent_find(results.begin(), results.end(),
                   [](const TaskResult &a, const TaskResult &b) {
                     return a.batchId + 1 == b.batchId;
                   }) != results.end();
}
