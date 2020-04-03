#ifndef PROOFOFCONCEPT_JOB_H
#define PROOFOFCONCEPT_JOB_H
#include "QueryTask.h"
#include "TaskResult.h"
#include <variant>

template <typename T0, typename ... Ts>
std::ostream & operator<< (std::ostream & s,
                           std::variant<T0, Ts...> const & v)
{ std::visit([&](auto && arg){ s << arg;}, v); return s; }

struct PassSegmentJob {
  explicit PassSegmentJob(TaskResult &&result);
  TaskResult result;

  friend std::ostream &operator<<(std::ostream &out, const PassSegmentJob &j);
};

using Job = std::variant<PassSegmentJob, QueryTask>;

#endif // PROOFOFCONCEPT_JOB_H
