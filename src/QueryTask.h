#ifndef PROOFOFCONCEPT_QUERYTASK_H
#define PROOFOFCONCEPT_QUERYTASK_H

#include <vector>
struct QueryTask {
  long batchId;
  char *data;
  int size;
#ifdef POC_DEBUG
  long startPos;
  long endPos;
#endif
};

#endif // PROOFOFCONCEPT_QUERYTASK_H
