//
// Created by mb4416 on 15/02/2020.
//

#ifndef PROOFOFCONCEPT_EXECUTOR_H
#define PROOFOFCONCEPT_EXECUTOR_H

#include "NodeCoordinator.h"
#include "YahooQuery.h"
class Executor {
  NodeCoordinator* coordinator;
  int ThreadNumber;
  YahooQuery* query;
public:
  Executor(NodeCoordinator *coordinator, int threadNumber, YahooQuery* query);

   void RunWorker(volatile bool &startRunning);
};

#endif // PROOFOFCONCEPT_EXECUTOR_H
