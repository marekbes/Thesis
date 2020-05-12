#ifndef PROOFOFCONCEPT_ABSTRACTNODECOORDINATOR_H
#define PROOFOFCONCEPT_ABSTRACTNODECOORDINATOR_H

#include "QueryTask.h"
template <typename TQuery> class AbstractNodeCoordinator {
public:
  virtual int GetNode() = 0;
  virtual void ProcessLocalResult(typename TQuery::TResult &&result) = 0;
  virtual void MarkWindowDone(long fromWindowId, long toWindowId,
                              int threadNumber) = 0;
  [[nodiscard]] virtual QueryTask GetJob() = 0;
};

#endif // PROOFOFCONCEPT_ABSTRACTNODECOORDINATOR_H
