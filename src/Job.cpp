#include "Job.h"
PassSegmentJob::PassSegmentJob(TaskResult &&result)
    : result(std::move(result)) {}

std::ostream &operator<<(std::ostream &out, const PassSegmentJob &j) {
  out << "PassSegment " << j.result;
  return out;
}

MarkBatchComplete::MarkBatchComplete(long batchId) : batchId(batchId) {}

std::ostream &operator<<(std::ostream &out, const MarkBatchComplete &j) {
  out << "MarkBatch " << j.batchId;
  return out;
}

MergeAndOutputJob::MergeAndOutputJob(
    size_t resultCount, TaskResult results[ResultGroup::RESULT_COUNT_LIMIT])
    : resultCount(resultCount), results() {
  std::move(results, results + resultCount, this->results);
}

std::ostream &operator<<(std::ostream &out, const MergeAndOutputJob &j) {
  for (size_t i = 0; i < j.resultCount; ++i) {
    out << "MergeAndOutput " << j.results[i] << ", ";
  }
  return out;
}