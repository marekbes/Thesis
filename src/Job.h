#ifndef PROOFOFCONCEPT_JOB_H
#define PROOFOFCONCEPT_JOB_H
#include "QueryTask.h"
#include "ResultGroup.h"
#include "TaskResult.h"
#include <variant>

template <typename T0, typename... Ts>
std::ostream &operator<<(std::ostream &s, std::variant<T0, Ts...> const &v) {
  std::visit([&](auto &&arg) { s << arg; }, v);
  return s;
}

struct PassSegmentJob {
  explicit PassSegmentJob(TaskResult &&result);
  TaskResult result;

  friend std::ostream &operator<<(std::ostream &out, const PassSegmentJob &j);
};

struct MergeAndOutputJob {
  explicit MergeAndOutputJob(
      size_t resultCount, TaskResult results[ResultGroup::RESULT_COUNT_LIMIT]);
  size_t resultCount;
  TaskResult results[ResultGroup::RESULT_COUNT_LIMIT];

  friend std::ostream &operator<<(std::ostream &out,
                                  const MergeAndOutputJob &j);
};

struct MarkBatchComplete {
  explicit MarkBatchComplete(long batchId);
  long batchId;
  friend std::ostream &operator<<(std::ostream &out,
                                  const MarkBatchComplete &j);
};

using Job = std::variant<int, PassSegmentJob, QueryTask, MergeAndOutputJob,
                         MarkBatchComplete>;

#endif // PROOFOFCONCEPT_JOB_H
