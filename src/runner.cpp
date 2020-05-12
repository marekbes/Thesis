#include "DelayedResultMerger.h"
#include "EagerResultMerger.h"
#include "Executor.h"
#include "NodeCoordinator.h"
#include "Setting.h"
#include "ThreadClockWindowMarker.h"
#include "ThreadCountWindowMarker.h"
#include <algorithm>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <numa.h>
#include <random>
#include <unordered_set>

#define MAIN_ON_NODE 1
unsigned int NodesUsed = 4;
unsigned int ThreadsPerNode = 8;
unsigned int ThreadCount = NodesUsed * ThreadsPerNode;
unsigned int RunLength = 0;

struct Configuration {
  bool operator==(const Configuration &rhs) const {
    return Query == rhs.Query && Merger == rhs.Merger && Marker == rhs.Marker;
  }
  bool operator!=(const Configuration &rhs) const { return !(rhs == *this); }
  std::string Query;
  std::string Merger;
  std::string Marker;
};

struct ConfigurationHash {
public:
  std::size_t operator()(const Configuration &x) const {
    return std::hash<std::string>()(x.Query) ^
           std::hash<std::string>()(x.Merger) ^
           std::hash<std::string>()(x.Marker);
  }
};

Configuration parse_args(int argc, const char **argv) {
  namespace po = boost::program_options;
  po::options_description desc("Allowed options");
  desc.add_options()("help", "produce help message")(
      "thread-count", po::value<int>(), "number of threads per node")(
      "run-length", po::value<int>(),
      "seconds before exiting")("nodes", po::value<int>(), "NUMA nodes used")(
      "query", po::value<std::string>()->default_value("yahoo"),
      "Query to run [yahoo]")("merger",
                              po::value<std::string>()->default_value("eager"),
                              "Result merger to use [eager | delayed]")(
      "marker", po::value<std::string>()->default_value("clock"),
      "Window marker to use [count | clock]");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    std::exit(1);
  }

  if (vm.count("nodes")) {
    NodesUsed = vm["nodes"].as<int>();
  }
  std::cout << "Number of nodes set to " << NodesUsed << ".\n";
  if (vm.count("thread-count")) {
    ThreadsPerNode = vm["thread-count"].as<int>();
  }
  ThreadCount = NodesUsed * ThreadsPerNode;
  std::cout << "Number of threads per node set to " << ThreadsPerNode << ".\n";
  std::cout << "Total number of threads set to " << ThreadCount << ".\n";
  if (vm.count("run-length")) {
    RunLength = vm["run-length"].as<int>();
    std::cout << "Running experiment for " << RunLength << ".\n";
  }
  return Configuration{vm["query"].as<std::string>(),
                       vm["merger"].as<std::string>(),
                       vm["marker"].as<std::string>()};
}

void reportThroughput() {
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

template <typename TQuery, template <typename, typename> typename TResultMerger,
          template <typename> typename TWindowMarker>
void start() {
  using _NodeCoordinator =
      NodeCoordinator<TQuery, TResultMerger, TWindowMarker>;
  using _Executor = Executor<TQuery>;
  std::vector<_NodeCoordinator *> coordinators;
  std::vector<_Executor *> workers;
  auto NodesUsed = 1 + ((ThreadCount - 1) / ThreadsPerNode);
  Setting::NODES_USED = NodesUsed;
  Setting::THREADS_USED = ThreadCount;
  auto ThreadsToAllocate = ThreadCount;
  auto staticData = Setting::Query::loadStaticData(NodesUsed);
  for (size_t i = 0; i < NodesUsed; i++) {
    numa_set_preferred(i);
    auto dataBuf = Setting::Query::loadData(i, NodesUsed);
    int status[1];
    long ret_code;
    status[0] = -1;
    ret_code =
        numa_move_pages(0 /*self memory */, 1, &dataBuf, NULL, status, 0);
    printf("Memory at %p is at %d node (retcode %ld)\n", dataBuf, status[0],
           ret_code);
    auto *coordinator = new _NodeCoordinator(i, dataBuf);
    coordinators.push_back(coordinator);
    for (size_t j = 0; j < std::min(ThreadsToAllocate, ThreadsPerNode); ++j) {
      auto *worker = new _Executor(coordinator, i * ThreadsPerNode + j,
                                   new Setting::Query(staticData));
      workers.push_back(worker);
    }
    ThreadsToAllocate -= ThreadsPerNode;
  }
  for (size_t i = 0; i < NodesUsed; i++) {
    numa_set_preferred(i);
    coordinators[i]->SetCoordinators(coordinators);
  }
  numa_set_preferred(MAIN_ON_NODE);

  boost::thread_group workerThreads;
  bool startRunning = false;
  for (unsigned int i = 0; i < ThreadCount; ++i) {
    auto thread = workerThreads.create_thread([i, &workers, &startRunning]() {
      workers[i]->RunWorker(startRunning);
    });
    auto name = "Worker " + std::to_string(i);
    pthread_setname_np(thread->native_handle(), name.c_str());
  }
  startRunning = true;
  reportThroughput();
}

using StarterFunc = std::function<void()>;

static std::unordered_map<Configuration, StarterFunc, ConfigurationHash>
    startMapping{
        {Configuration{"yahoo", "eager", "count"},
         start<YahooQuery, EagerResultMerger, ThreadCountWindowMarker>},
        {Configuration{"yahoo", "eager", "clock"},
         start<YahooQuery, EagerResultMerger, ThreadClockWindowMarker>},
        {Configuration{"yahoo", "delayed", "count"},
         start<YahooQuery, DelayedResultMerger, ThreadCountWindowMarker>},
        {Configuration{"yahoo", "delayed", "clock"},
         start<YahooQuery, DelayedResultMerger, ThreadClockWindowMarker>},
    };

int main(int argc, const char **argv) {
  assert(Setting::BATCH_SIZE % Setting::PAGE_SIZE == 0);
  numa_set_bind_policy(1);
  numa_set_preferred(MAIN_ON_NODE);
  auto fromNodes = numa_all_nodes_ptr;
  auto toNodes = numa_parse_nodestring("1");
  numa_migrate_pages(0, fromNodes, toNodes);
  numa_free_nodemask(fromNodes);
  numa_free_nodemask(toNodes);

  auto configuration = parse_args(argc, argv);
  std::cout << "Running query: '" << configuration.Query << "' merger: '"
            << configuration.Merger << "' marker: '" << configuration.Marker
            << "'" << std::endl;
  startMapping[configuration]();
}