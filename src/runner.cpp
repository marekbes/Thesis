#include "Executor.h"
#include "NodeCoordinator.h"
#include "Setting.h"
#include <algorithm>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <fstream>
#include <iostream>
#include <numa.h>
#include <random>
#include <unordered_set>

#define MAIN_ON_NODE 2
unsigned int NodesUsed = 2;
unsigned int ThreadsPerNode = 5;
unsigned int ThreadCount = NodesUsed * ThreadsPerNode;
unsigned int RunLength = 0;

void parse_args(int argc, const char **argv) {
  namespace po = boost::program_options;
  po::options_description desc("Allowed options");
  desc.add_options()("help", "produce help message")(
      "thread-count", po::value<int>(), "")("run-length", po::value<int>(),
                                            "seconds before exiting");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    std::exit(1);
  }

  if (vm.count("thread-count")) {
    ThreadCount = vm["thread-count"].as<int>();
  }
  std::cout << "Number of threads set to " << ThreadCount << ".\n";
  if (vm.count("run-length")) {
    RunLength = vm["run-length"].as<int>();
    std::cout << "Running experiment for " << RunLength << ".\n";
  }
}

std::vector<long> loadStaticData(int nodeCount) {
  std::ifstream file(Setting::DATA_PATH + "input_" + std::to_string(nodeCount) +
                         ".dat",
                     std::ifstream::binary);
  if (file.fail()) {
    throw std::invalid_argument("missing input data");
  }
  std::vector<long> data;
  unsigned int len = 0;
  file.read((char *)&len, sizeof(len));
  data.resize(len);
  if (len > 0)
    file.read((char *)&data[0], len * sizeof(long));
  return data;
}

void *loadData(int node, int nodeCount) {
  std::ifstream file(Setting::DATA_PATH + "input_" + std::to_string(node) +
                         "_" + std::to_string(nodeCount) + ".dat",
                     std::ifstream::ate | std::ifstream::binary);
  if (file.fail()) {
    throw std::invalid_argument("missing input data");
  }
  auto size = file.tellg();
  file.seekg(0);
  assert(size >= static_cast<long>(Setting::DATA_COUNT * Setting::BATCH_SIZE));
  auto data = numa_alloc_onnode(size, node);
  file.read(static_cast<char *>(data), size);
  return data;
}

int main(int argc, const char **argv) {
  assert(Setting::BATCH_SIZE % Setting::PAGE_SIZE == 0);
  numa_set_bind_policy(1);
  numa_set_preferred(MAIN_ON_NODE);
  auto fromNodes = numa_parse_nodestring("0-3");
  auto toNodes = numa_parse_nodestring("2");
  numa_migrate_pages(0, fromNodes, toNodes);
  numa_free_nodemask(fromNodes);
  numa_free_nodemask(toNodes);

  parse_args(argc, argv);
  std::vector<NodeCoordinator *> coordinators;
  std::vector<Executor *> workers;
  std::vector<NodeComm *> comms;
  auto NodesUsed = 1 + ((ThreadCount - 1) / ThreadsPerNode);
  Setting::NODES_USED = NodesUsed;
  auto ThreadsToAllocate = ThreadCount;
  auto staticData = loadStaticData(NodesUsed);
  for (size_t i = 0; i < NodesUsed; i++) {
    numa_set_preferred(i);
    auto dataBuf = loadData(i, NodesUsed);
    int status[1];
    long ret_code;
    status[0] = -1;
    ret_code =
        numa_move_pages(0 /*self memory */, 1, &dataBuf, NULL, status, 0);
    printf("Memory at %p is at %d node (retcode %ld)\n", dataBuf, status[0],
           ret_code);
    auto *coordinator = new NodeCoordinator(i, dataBuf);
    coordinators.push_back(coordinator);
    for (size_t j = 0; j < std::min(ThreadsToAllocate, ThreadsPerNode); ++j) {
      auto *worker = new Executor(coordinator, i * ThreadsPerNode + j,
                                  new YahooQuery(staticData));
      workers.push_back(worker);
    }
    comms.push_back(&coordinator->NodeComms);
    ThreadsToAllocate -= ThreadsPerNode;
  }
  for (size_t i = 0; i < NodesUsed; i++) {
    numa_set_preferred(i);
    coordinators[i]->NodeComms.initComms(comms);
  }
  numa_set_preferred(MAIN_ON_NODE);

  boost::thread_group workerThreads;
  bool startRunning = false;
  for (unsigned int i = 0; i < ThreadCount; ++i) {
    auto thread = workerThreads.create_thread(
        [i, &workers, &startRunning]() { workers[i]->RunWorker(startRunning); });
    pthread_setname_np(thread->native_handle(), "Worker");
  }
  startRunning = true;

  // Report throughput
  auto reportTime = RunLength <= 0 ? 2 : RunLength;
  boost::this_thread::sleep_for(boost::chrono::seconds(2));
  Setting::DataCounter.exchange(0, boost::memory_order_relaxed);
  while (true) {
    boost::this_thread::sleep_for(boost::chrono::seconds(reportTime));
    long throughput =
        Setting::DataCounter.exchange(0, boost::memory_order_relaxed);
    std::cout << "throughput: " << (double)throughput / reportTime / 1000000000
              << "GB/s" << std::endl;
    if (RunLength > 0) {
      exit(0);
    }
  }
}