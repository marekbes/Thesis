#ifndef PROOFOFCONCEPT_EXECUTOR_H
#define PROOFOFCONCEPT_EXECUTOR_H

#include "Executor.h"
#include "NodeCoordinator.h"
#include "queries/YahooQuery.h"
#include <iostream>
#include <numa.h>
#include <sstream>
template <class TQuery> class Executor {
  NodeCoordinator<TQuery> *coordinator;
  int ThreadNumber;
  TQuery *query;
  int lastWindowId = 0;

public:
  Executor(NodeCoordinator<TQuery> *coordinator, int threadNumber,
           TQuery *query)
      : coordinator(coordinator), ThreadNumber(threadNumber), query(query) {}

  void RunWorker(volatile bool &startRunning) {
    while (!startRunning) {
    }
    numa_set_preferred(this->coordinator->GetNode());

    {
      std::stringstream stream;
      stream << "thread " << ThreadNumber << " starting" << std::endl;
      std::cout << stream.str();
    }
    auto nodeCpuMask = numa_allocate_cpumask();
    numa_node_to_cpus(this->coordinator->GetNode(), nodeCpuMask);
    auto skipCPUs = (ThreadNumber % 8) + 1;
    for (unsigned int i = 0; i < sizeof(cpu_set_t) * 8; ++i) {
      if (numa_bitmask_isbitset(nodeCpuMask, i)) {
        skipCPUs--;
        if (skipCPUs == 0) {
          numa_bitmask_clearall(nodeCpuMask);
          numa_bitmask_setbit(nodeCpuMask, i);

          {
            std::stringstream stream;
            stream << "thread " << ThreadNumber << " bound to core " << i
                   << std::endl;
            std::cout << stream.str();
          }
          numa_sched_setaffinity(0, nodeCpuMask);
        }
      }
    }
    query->SetOutputCb([this](typename TQuery::TResult &&tr) {
      for (; lastWindowId < tr.windowId; ++lastWindowId) {
        coordinator->markWindowDone(lastWindowId);
      }
      this->coordinator->ProcessLocalResult(std::move(tr));
    });
    while (true) {
      auto task = coordinator->GetJob();
      query->process(task);
    }
  }
};

#endif // PROOFOFCONCEPT_EXECUTOR_H
