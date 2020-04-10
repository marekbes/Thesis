#include "Job.h"
PassSegmentJob::PassSegmentJob(TaskResult &&result)
    : result(std::move(result)) {}

std::ostream &operator<<(std::ostream &out, const PassSegmentJob &j) {
  out << j.result;
  return out;
}

MergeAndOutputJob::MergeAndOutputJob(ResultGroup &&group) : group(std::move(group)) {}

std::ostream &operator<<(std::ostream &out, const MergeAndOutputJob &j) {
  out << j.group;
  return out;
}