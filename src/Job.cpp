#include "Job.h"
PassSegmentJob::PassSegmentJob(TaskResult &&result) : result(std::move(result)) {}

std::ostream &operator<<(std::ostream &out, const PassSegmentJob &j) {
  out << j.result;
  return out;
}