#ifndef PROOFOFCONCEPT_JOB_H
#define PROOFOFCONCEPT_JOB_H
#include "QueryTask.h"
#include "TaskResult.h"
#include <variant>

struct PassSegmentJob
{
  explicit PassSegmentJob(TaskResult &&result);
  TaskResult result;
};

using Job = std::variant<PassSegmentJob, QueryTask>;


#endif // PROOFOFCONCEPT_JOB_H
