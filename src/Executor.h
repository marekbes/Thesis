#ifndef PROOFOFCONCEPT_EXECUTOR_H
#define PROOFOFCONCEPT_EXECUTOR_H

#include "AbstractNodeCoordinator.h"
#include "Executor.h"
#include "NodeCoordinator.h"
#include <iostream>
#include <numa.h>
#include <sstream>

template <typename TQuery> class Executor {
  AbstractNodeCoordinator<TQuery> *coordinator;
  int ThreadNumber;
  TQuery *query;
  long lastWindowId = 0;

public:
  Executor(AbstractNodeCoordinator<TQuery> *coordinator, int threadNumber,
           TQuery *query)
      : coordinator(coordinator), ThreadNumber(threadNumber), query(query) {}

  void RunWorker(volatile bool &startRunning) {
    while (!startRunning) {
    }
    numa_set_preferred(coordinator->GetNode());
    auto nodeCpuMask = numa_allocate_cpumask();
    numa_node_to_cpus(coordinator->GetNode(), nodeCpuMask);
    auto skipCPUs = (ThreadNumber % 8) + 1;
    for (unsigned int i = 0; i < sizeof(cpu_set_t) * 8; ++i) {
      if (numa_bitmask_isbitset(nodeCpuMask, i)) {
        skipCPUs--;
        if (skipCPUs == 0) {
          numa_bitmask_clearall(nodeCpuMask);
          numa_bitmask_setbit(nodeCpuMask, i);
          {
            std::stringstream stream;
            stream << "thread " << ThreadNumber << " running on core " << i
                   << std::endl;
            std::cout << stream.str();
          }
          numa_sched_setaffinity(0, nodeCpuMask);
        }
      }
    }
    numa_free_cpumask(nodeCpuMask);
    query->SetOutputCb([this](typename TQuery::TResult &&tr) {
      coordinator->MarkWindowDone(lastWindowId, tr.windowId, ThreadNumber);
      lastWindowId = std::max(tr.windowId, lastWindowId);
      coordinator->ProcessLocalResult(std::move(tr));
    });
    while (startRunning) {
      auto task = coordinator->GetJob();
      if (task.batchId != -1) {
        query->process(task);
      }
    }
  }
};

#endif // PROOFOFCONCEPT_EXECUTOR_H
