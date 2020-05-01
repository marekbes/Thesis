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
