#include "Executor.h"
#include <iostream>
#include <numa.h>

Executor::Executor(NodeCoordinator *coordinator, int threadNumber,
                   YahooQuery *query)
    : coordinator(coordinator), ThreadNumber(threadNumber), query(query) {}

void Executor::RunWorker() {
  numa_set_preferred(this->coordinator->GetNode());
  std::cout << "thread " << ThreadNumber << " starting" << std::endl;
  auto nodeCpuMask = numa_allocate_cpumask();
  numa_node_to_cpus(this->coordinator->GetNode(), nodeCpuMask);
  //    if (numa_bitmask_weight(nodeCpuMask) < ThreadCount) {
  //      std::cout << "too many threads for one numa node!" << std::endl;
  //      return;
  //    }
  auto skipCPUs = (ThreadNumber % 8) + 1;
  for (unsigned int i = 0; i < sizeof(cpu_set_t) * 8; ++i) {
    if (numa_bitmask_isbitset(nodeCpuMask, i)) {
      skipCPUs--;
      if (skipCPUs == 0) {
        numa_bitmask_clearall(nodeCpuMask);
        numa_bitmask_setbit(nodeCpuMask, i);
        std::cout << "thread " << ThreadNumber << " bound to core " << i
                  << std::endl;
        numa_sched_setaffinity(0, nodeCpuMask);
      }
    }
  }
  query->SetOutputCb(std::bind(&NodeCoordinator::ProcessLocalResult, coordinator,
                               std::placeholders::_1));
  while (true) {
    auto job = coordinator->GetJob();
    if (std::holds_alternative<QueryTask>(job)) {
      query->process(std::get<QueryTask>(job));
    } else
    {
      coordinator->ProcessJob(job);
    }
  }
}