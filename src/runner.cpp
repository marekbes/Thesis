#include "Executor.h"
#include "NodeCoordinator.h"
#include "ObjectPool.h"
#include "Semaphore.h"
#include "Setting.h"
#include <algorithm>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <numa.h>
#include <random>
#include <unordered_set>

#define TUPLE_COUNT 1024 * 2
#define MAIN_ON_NODE 2
std::vector<char> *StaticJoinData;
unsigned int ThreadsPerNode = 1;
unsigned int ThreadCount = 2;
unsigned int RunLength = 0;
unsigned int SegmentCount = 1;

std::vector<long> generateStaticData() {
  std::random_device rd;
  std::mt19937_64 eng(rd());
  std::uniform_int_distribution<long> distr(0, 1000000);
  std::unordered_set<long> set;
  auto staticData = std::vector<long>(2 * sizeof(long) * 1024);
  auto staticBuf = staticData.data();

  long campaign_id = distr(eng); // 0;
  set.insert(campaign_id);
  for (long i = 0; i < 1000; ++i) {
    if (i > 0 && i % 10 == 0) {
      campaign_id = distr(eng); //++;
      bool is_in = set.find(campaign_id) != set.end();
      while (is_in) {
        campaign_id = distr(eng);
        is_in = set.find(campaign_id) != set.end();
      }
    }
    staticBuf[i * 2] = i;
    staticBuf[i * 2 + 1] = (long)campaign_id;
  }
  return staticData;
}

void generateData(void *data, long offset, long nodes,
                  const std::vector<long> staticBuf, long campaignKeys,
                  int len) {
  std::random_device rd;
  std::mt19937_64 eng(rd());
  std::uniform_int_distribution<long> distr(0, 1000000);
  auto buf = (InputSchema *)data;

  std::string line;
  auto user_id = distr(eng);
  auto page_id = distr(eng);
  int pos = 0;
  for (int i = 0; i < len; i += Setting::BATCH_COUNT * nodes) {
    long idx = i + offset;
    while (idx < Setting::BATCH_SIZE * nodes) {
      auto ad_id = staticBuf[((idx % 100000) % campaignKeys) * 2];
      auto ad_type = (idx % 100000) % 5;
      auto event_type = (idx % 100000) % 3;
      buf[pos].timestamp = idx;
      buf[pos].user_id = user_id;
      buf[pos].page_id = page_id;
      buf[pos].ad_id = ad_id;
      buf[pos].ad_type = ad_type;
      buf[pos].event_type = event_type;
      buf[pos].ip_address = -1;
      pos++;
      idx++;
    }
  }
}

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

int main(int argc, const char **argv) {
  assert(Setting::BATCH_SIZE % Setting::PAGE_SIZE == 0);
  //  numa_set_bind_policy(1);
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
  auto staticData = generateStaticData();
  for (size_t i = 0; i < NodesUsed; i++) {
    numa_set_preferred(i);
    auto dataBuf =
        numa_alloc_onnode(Setting::BATCH_SIZE * Setting::DATA_COUNT, i);
    generateData(dataBuf, i * Setting::BATCH_COUNT, NodesUsed, staticData, 1000,
                 Setting::BATCH_COUNT * Setting::DATA_COUNT);
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
    comms.push_back(&coordinator->GetComm());
    ThreadsToAllocate -= ThreadsPerNode;
  }
  for (size_t i = 0; i < NodesUsed; i++) {
    numa_set_preferred(i);
    coordinators[i]->GetComm().initComms(comms);
  }
  numa_set_preferred(MAIN_ON_NODE);

  boost::thread_group workerThreads;
  for (unsigned int i = 0; i < ThreadCount; ++i) {
    auto thread = workerThreads.create_thread(
        boost::bind(&Executor::RunWorker, workers[i]));
    pthread_setname_np(thread->native_handle(), "Worker");
  }

  // Report throughput
  auto reportTime = RunLength <= 0 ? 2 : RunLength;
  boost::this_thread::sleep_for(boost::chrono::seconds(2));
  Setting::DataCounter.exchange(0, boost::memory_order_relaxed);
  while (true) {
    boost::this_thread::sleep_for(boost::chrono::seconds(reportTime));
    long throughput =
        Setting::DataCounter.exchange(0, boost::memory_order_relaxed);
    std::cout << "throughput: "
              << (double)throughput / reportTime / 1000000000 << "GB/s"
              << std::endl;
    if (RunLength > 0) {
      exit(0);
    }
  }
}