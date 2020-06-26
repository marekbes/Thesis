#include "DefaultTableProvider.h"
#include "DelayedResultMerger.h"
#include "DirectResultMerger.h"
#include "EagerResultMerger.h"
#include "Executor.h"
#include "NodeCoordinator.h"
#include "Setting.h"
#include "ThreadClockWindowMarker.h"
#include "ThreadCountWindowMarker.h"
#include "queries/YahooQuery.h"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <numa.h>
#include <random>
#include <unordered_set>

#define MAIN_ON_NODE 1
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
      "thread-count", po::value<int>()->default_value(8),
      "number of threads per node")("run-length", po::value<int>(),
                                    "seconds before exiting")(
      "nodes", po::value<int>()->default_value(4), "NUMA nodes used")(
      "query", po::value<std::string>()->default_value("yahoo"),
      "Query to run [yahoo]")(
      "merger", po::value<std::string>()->default_value("direct"),
      "Result merger to use [eager | delayed | direct]")(
      "marker", po::value<std::string>()->default_value("count"),
      "Window marker to use [count | clock]")(
      "window-size", po::value<int>()->default_value(100000),
      "Size of window to output")(
      "window-slide", po::value<int>()->default_value(10000),
      "Window slide, tumbling if equal to window size")(
      "batch-size", po::value<int>(),
      "Number of elements in a batch")(
      "shared-buffer-size", po::value<int>(),
      "Number of slots in the shared buffer")(
      "input", po::value<std::string>(),
      "Path to directory containing input");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    std::exit(1);
  }

  Setting::NODES_USED = vm["nodes"].as<int>();
  Setting::THREADS_USED = Setting::NODES_USED * vm["thread-count"].as<int>();
  std::cout << "Running on " << Setting::NODES_USED << " nodes using "
            << Setting::THREADS_USED << " threads.\n";
  if (vm.count("run-length")) {
    RunLength = vm["run-length"].as<int>();
    std::cout << "Running experiment for " << RunLength << ".\n";
  }
  Setting::WINDOW_SLIDE = vm["window-slide"].as<int>();
  Setting::WINDOW_SIZE = vm["window-size"].as<int>();
  std::cout << "Window slide set to " << Setting::WINDOW_SLIDE
            << " and size to " << Setting::WINDOW_SIZE << ".\n";

  if (vm.count("batch-size")) {
    Setting::BATCH_COUNT = vm["batch-size"].as<int>();
  }
  std::cout << "Batch size set to " << Setting::BATCH_COUNT << " elements.\n";
  if (vm.count("shared-buffer-size")) {
    Setting::SHARED_SLOTS = vm["shared-buffer-size"].as<int>();
  }
  std::cout << "Shared buffer has " << Setting::SHARED_SLOTS << " slots.\n";
  if (vm.count("input")) {
    Setting::DATA_PATH = vm["input"].as<std::string>();
  }
  std::cout << "Input path: '" << Setting::DATA_PATH << "'.\n";

  return Configuration{
      boost::algorithm::to_lower_copy(vm["query"].as<std::string>()),
      boost::algorithm::to_lower_copy(vm["merger"].as<std::string>()),
      boost::algorithm::to_lower_copy(vm["marker"].as<std::string>())};
}

void reportThroughput(boost::thread_group &group, bool &running) {
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
      LatencyMonitor::DisableCollection();
      running = false;
      group.join_all();
      LatencyMonitor::PrintStatistics();
      exit(0);
    }
  }
}

template <template <typename> typename TQuery,
          template <template <typename> typename, typename>
          typename TTableProvider,
          template <typename, typename> typename TResultMerger,
          template <typename> typename TWindowMarker>
void start() {
  numa_set_preferred(MAIN_ON_NODE);
  using Types =
      NodeCoordinator<TQuery, TTableProvider, TResultMerger, TWindowMarker>;
  using _NodeCoordinator = typename Types::Impl;
  _NodeCoordinator::slider.Init();
  using Query = typename Types::Query;
  Setting::BATCH_SIZE =
      Setting::BATCH_COUNT * sizeof(typename Query::InputSchema);
  using _Executor = Executor<Query>;
  std::vector<_NodeCoordinator *> coordinators;
  std::vector<_Executor *> workers;
  auto NodesUsed = Setting::NODES_USED;
  _NodeCoordinator::InitializeSharedRegion();
  auto ThreadsToAllocate = Setting::THREADS_USED;
  auto ThreadsPerNode = ThreadsToAllocate / NodesUsed;
  auto staticData = Query::loadStaticData(NodesUsed);
  for (size_t i = 0; i < NodesUsed; i++) {
    numa_set_preferred(i);
    auto dataBuf = Query::loadData(i, NodesUsed);
    int status[1];
    long ret_code;
    status[0] = -1;
    ret_code =
        numa_move_pages(0 /*self memory */, 1, &dataBuf, NULL, status, 0);
    printf("Memory at %p is at %d node (retcode %ld)\n", dataBuf, status[0],
           ret_code);
    auto *coordinator = new typename _NodeCoordinator::Impl(i, dataBuf);
    coordinators.push_back(coordinator);
    for (size_t j = 0; j < std::min(ThreadsToAllocate, ThreadsPerNode); ++j) {
      auto *worker = new _Executor(
          coordinator, i * ThreadsPerNode + j,
          new Query(staticData, typename Types::TableProvider(*coordinator)));
      workers.push_back(worker);
    }
    ThreadsToAllocate -= ThreadsPerNode;
  }
  for (size_t i = 0; i < NodesUsed; i++) {
    numa_set_preferred(i);
    coordinators[i]->SetCoordinators(coordinators);
  }

  boost::thread_group workerThreads;
  bool startRunning = false;
  for (unsigned int i = 0; i < Setting::THREADS_USED; ++i) {
    auto thread = workerThreads.create_thread([i, &workers, &startRunning]() {
      workers[i]->RunWorker(startRunning);
    });
    auto name = "Worker " + std::to_string(i);
    pthread_setname_np(thread->native_handle(), name.c_str());

    startRunning = true;
  }
  reportThroughput(workerThreads, startRunning);
}

using StarterFunc = std::function<void()>;

static std::unordered_map<Configuration, StarterFunc, ConfigurationHash>
    startMapping{
        {Configuration{"yahoo", "eager", "count"},
         start<YahooQuery, DefaultTableProvider, EagerResultMerger,
               ThreadCountWindowMarker>},
        {Configuration{"yahoo", "eager", "clock"},
         start<YahooQuery, DefaultTableProvider, EagerResultMerger,
               ThreadClockWindowMarker>},
        {Configuration{"yahoo", "delayed", "count"},
         start<YahooQuery, DefaultTableProvider, DelayedResultMerger,
               ThreadCountWindowMarker>},
        {Configuration{"yahoo", "delayed", "clock"},
         start<YahooQuery, DefaultTableProvider, DelayedResultMerger,
               ThreadClockWindowMarker>},
        {Configuration{"yahoo", "direct", "count"},
         start<YahooQuery, DirectTableProvider, DirectResultMerger,
               ThreadCountWindowMarker>},
        {Configuration{"yahoo", "direct", "clock"},
         start<YahooQuery, DirectTableProvider, DirectResultMerger,
               ThreadClockWindowMarker>},
    };

int main(int argc, const char **argv) {
  assert(Setting::BATCH_SIZE % Setting::PAGE_SIZE == 0);
  numa_set_bind_policy(1);
  auto fromNodes = numa_all_nodes_ptr;
  auto toNodes = numa_parse_nodestring("1");
  numa_migrate_pages(0, fromNodes, toNodes);

  auto configuration = parse_args(argc, argv);
  std::cout << "Running query: '" << configuration.Query << "' merger: '"
            << configuration.Merger << "' marker: '" << configuration.Marker
            << "'" << std::endl;
  startMapping[configuration]();
}