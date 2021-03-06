#include "HashTable.h"
#include "ObjectPool.h"
#include "Semaphore.h"
#include "semaphore.h"
#include <boost/thread.hpp>
#include <iostream>
#include <iterator>
#include <mutex>
#include <numa.h>
#include <pthread.h>
#include <random>
#include <unordered_set>
#include <boost/program_options.hpp>

bool debug = true;
#define TUPLE_COUNT 1024 * 2
#define NUMA_NODE 2

std::vector<char> *StaticJoinData;
boost::atomic_int64_t DataCounter(0);
unsigned int ThreadCount = 28;
unsigned int RunLength = 0;
unsigned int SegmentCount = 1;

char output[1024];
void *outputBuf = output;

struct InputSchema_128
{
  long timestamp;
  long padding_0;
  long user_id;
  long page_id;
  long ad_id;
  long ad_type;
  long event_type;
  long ip_address;
  static void parse(InputSchema_128 &tuple, std::string &line)
  {
    std::istringstream iss(line);
    std::vector<std::string> words{std::istream_iterator<std::string>{iss},
                                   std::istream_iterator<std::string>{}};
    tuple.timestamp = std::stol(words[0]);
    tuple.user_id = std::stoul(words[1]);
    tuple.page_id = std::stoul(words[2]);
    tuple.ad_id = std::stoul(words[3]);
    tuple.ad_type = std::stoul(words[4]);
    tuple.event_type = std::stoul(words[5]);
    tuple.ip_address = std::stoul(words[6]);
  }
};

std::vector<char> *generateStaticData()
{
  std::random_device rd;
  std::mt19937_64 eng(rd());
  std::uniform_int_distribution<long> distr(0, 1000000);
  std::unordered_set<long> set;
  auto staticData = new std::vector<char>(2 * sizeof(long) * 1024);
  auto staticBuf = (long *)staticData->data();

  long campaign_id = distr(eng); // 0;
  set.insert(campaign_id);
  for (unsigned long i = 0; i < 1000; ++i)
  {
    if (i > 0 && i % 10 == 0)
    {
      campaign_id = distr(eng); //++;
      bool is_in = set.find(campaign_id) != set.end();
      while (is_in)
      {
        campaign_id = distr(eng);
        is_in = set.find(campaign_id) != set.end();
      }
    }
    staticBuf[i * 2] = i;
    staticBuf[i * 2 + 1] = (long)campaign_id;
  }
  return staticData;
}

std::vector<char> *generateData(long *staticBuf, long campaignKeys)
{
  std::random_device rd;
  std::mt19937_64 eng(rd());
  std::uniform_int_distribution<long> distr(0, 1000000);
  size_t len = sizeof(InputSchema_128) * TUPLE_COUNT;
  auto data = new std::vector<char>(len);
  auto buf = (InputSchema_128 *)data->data();

  std::string line;
  auto user_id = distr(eng);
  auto page_id = distr(eng);
  unsigned long idx = 0;
  while (idx < len / sizeof(InputSchema_128))
  {
    auto ad_id = staticBuf[((idx % 100000) % campaignKeys) * 2];
    auto ad_type = (idx % 100000) % 5;
    auto event_type = (idx % 100000) % 3;
    buf[idx].timestamp = idx;
    buf[idx].user_id = user_id;
    buf[idx].page_id = page_id;
    buf[idx].ad_id = ad_id;
    buf[idx].ad_type = ad_type;
    buf[idx].event_type = event_type;
    buf[idx].ip_address = -1;
    idx++;
  }

  //  if (debug) {
  //    std::cout << "timestamp user_id page_id ad_id ad_type event_type
  //    ip_address"
  //              << std::endl;
  //    for (unsigned long i = 0; i < data->size() / sizeof(InputSchema_128);
  //    ++i) {
  //      printf("[DBG] %09lu: %7ld %13ld %8ld %13ld %3ld %6ld %2ld \n", i,
  //             buf[i].timestamp, (long)buf[i].user_id, (long)buf[i].page_id,
  //             (long)buf[i].ad_id, buf[i].ad_type, buf[i].event_type,
  //             (long)buf[i].ip_address);
  //    }
  //  }
  return data;
};

void updateTimeStamps(InputSchema_128 *input, int count, long offset)
{
  for (int i = 0; i < count; ++i)
  {
    input[i].timestamp += offset;
  }
}

struct CounterVal
{
  long _1;
  long _2;
};

std::shared_ptr<HashTable<long, long>> loadStaticJoinTable()
{
  auto *dataPtr = reinterpret_cast<long *>(StaticJoinData->data());
  auto table = std::make_shared<HashTable<long, long>>(2048);
  for (int i = 0; i < 1000; ++i)
  {
    table->insert(dataPtr[i * 2], dataPtr[i * 2 + 1], 0);
  }
  return table;
}

template <typename KeyT, typename ValueT, typename HashT, typename EqT,
          typename AggrT>
class HashTableInitializer
{
public:
  void operator()(HashTable<KeyT, ValueT, HashT, EqT, AggrT> *element)
  {
    element->clear();
  }
};

template <typename KeyT, typename ValueT, typename HashT, typename EqT,
          typename AggrT>
class HashTableReleaser
{
public:
  void operator()(HashTable<KeyT, ValueT, HashT, EqT, AggrT> *) {}
};

#define HASH_TABLE_POOL_SIZE 100

template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>,
          typename EqT = HashMapEqualTo<KeyT>,
          typename AggrT = DummyAggr<ValueT>>
class HashTablePool
    : public StackObjectPool<
          HashTable<KeyT, ValueT, HashT, EqT, AggrT>, HASH_TABLE_POOL_SIZE,
          HashTableInitializer<KeyT, ValueT, HashT, EqT, AggrT>,
          HashTableReleaser<KeyT, ValueT, HashT, EqT, AggrT>>
{
};

struct Task
{
  int segment;
  long window;
  std::vector<char> *data;
  semaphore done;
};

struct ResultBuilder
{
private:
  struct ResultSegment
  {
    HashTable<long, CounterVal> *result;
    HashTablePool<long, CounterVal> *pool;
  };

  struct ResultBundle
  {
    ResultBundle()
        : counter(0), segments(SegmentCount), canBeOutputed(false),
          canBeClaimed(true) {}
    bool canBeClaimed;
    bool canBeOutputed;
    long window;
    boost::atomic_int counter;
    std::vector<ResultSegment> segments;
  };

  std::vector<ResultBundle> bundles =
      std::vector<ResultBundle>(HASH_TABLE_POOL_SIZE * 3);
  std::mutex outputLock;
  int currentOutputWindowId = 0;

public:
  void AddResult(const Task &task, HashTable<long, CounterVal> *result,
                 HashTablePool<long, CounterVal> *pool)
  {
    auto &bundle = bundles[task.window % bundles.size()];
    while (bundle.window != task.window) {
      assert(bundle.canBeClaimed);
      auto segmentsCounter =
          bundle.counter.exchange(SegmentCount, boost::memory_order_relaxed);
      if (segmentsCounter == 0) {
        if (debug) {
          for (auto &resultSegment : bundle.segments) {
            resultSegment.result = nullptr;
            resultSegment.pool = nullptr;
          }
        }
        bundle.window = task.window;
        bundle.canBeClaimed = false;
        bundle.canBeOutputed = false;
      }
    }
    bundle.segments[task.segment].pool = pool;
    bundle.segments[task.segment].result = result;

    auto segmentsMissing =
        bundle.counter.fetch_add(-1, boost::memory_order_relaxed);
    if (segmentsMissing == 1) {
      bundle.canBeOutputed = true;
      if (outputLock.try_lock()) {
        while (bundles[currentOutputWindowId % bundles.size()].canBeOutputed) {
          auto &bundle = bundles[currentOutputWindowId % bundles.size()];
          assert(!bundle.canBeClaimed);
          for (unsigned int i = 0; i < SegmentCount; ++i) {
            auto table = bundle.segments[i].result;
            for (int j = 0; j < table->getNumberOfBuckets(); ++j) {
              auto bucket = table->getBuckets()[j];
              if (bucket.state) {
                *(long *)outputBuf = bucket.key;
                *(long *)outputBuf = bucket.value._1;
              }
            }
            bundle.segments[i].pool->release(table);
          }
          bundle.canBeOutputed = false;
          bundle.canBeClaimed = true;
          currentOutputWindowId++;
        }
        outputLock.unlock();
      }
    }
    DataCounter.fetch_add(TUPLE_COUNT * sizeof(InputSchema_128),
                          boost::memory_order_relaxed);
  }
};

ResultBuilder resultThread;

struct GeneratorThread
{
private:
  static const int TaskCount = 100;
  std::mutex mtx;
  semaphore tasksAvailable;
  Task tasks[TaskCount];
  boost::atomic_int taskCounter{};

public:
  GeneratorThread() : tasksAvailable() {}
  Task &getTask() {
    tasksAvailable.down();
    auto taskId = taskCounter.fetch_add(1, boost::memory_order_relaxed);
    return tasks[taskId % TaskCount];
  }

  void markTaskDone(Task &task) { task.done.up(); }

  void runGenerator()
  {
    numa_run_on_node(NUMA_NODE);
    numa_set_preferred(NUMA_NODE);
    for (int i = 0; i < TaskCount; ++i)
    {

      auto &task = tasks[i % TaskCount];
      task.window = i / SegmentCount;
      task.segment = i % SegmentCount;
      task.data = generateData((long *)StaticJoinData->data() +
                                   (1000 / SegmentCount * (i % SegmentCount)),
                               1000 / SegmentCount);
    tasksAvailable.up();
    }
    std::cout << "generated all data" << std::endl;
    auto currentTaskId = TaskCount;
    while (true) {
      tasks[currentTaskId % TaskCount].done.down();
      updateTask(currentTaskId);
      currentTaskId++;
    }
  }

  void updateTask(int taskId)
  {
    auto &task = tasks[taskId % TaskCount];
    task.window = taskId / SegmentCount;
    task.segment = taskId % SegmentCount;
    // updateTimeStamps((InputSchema_128 *)task.data->data(), TUPLE_COUNT,
    // TUPLE_COUNT);
    //tasksAvailable.up();
  }
};

struct WorkerThread
{
  std::shared_ptr<HashTable<long, long>> StaticJoinTable;
  HashTable<long, CounterVal> *CurrentCountMap{};
  std::vector<char> *GeneratedData{};
  HashTablePool<long, CounterVal> TablePool;
  int ThreadNumber;
  std::shared_ptr<GeneratorThread> Generator;

  WorkerThread(int number, std::shared_ptr<GeneratorThread> generator)
      : ThreadNumber(number), Generator(generator){

                              };

  void RunWorker()
  {
    auto node = ThreadNumber / 8;
    numa_set_preferred(node);
    std::cout << "thread " << ThreadNumber << " starting" << std::endl;
    auto nodeCpuMask = numa_allocate_cpumask();
    numa_node_to_cpus(node, nodeCpuMask);
    auto skipCPUs = (ThreadNumber % 8) + 1;
    for (unsigned int i = 0; i < sizeof(cpu_set_t) * 8; ++i)
    {
      if (numa_bitmask_isbitset(nodeCpuMask, i))
      {
        skipCPUs--;
        if (skipCPUs == 0)
        {
          numa_bitmask_clearall(nodeCpuMask);
          numa_bitmask_setbit(nodeCpuMask, i);
          std::cout << "thread " << ThreadNumber << " bound to core " << i << std::endl;
          numa_sched_setaffinity(0, nodeCpuMask);
        }
      }
    }
    StaticJoinTable = loadStaticJoinTable();
    CurrentCountMap = TablePool.acquire();
    while (true)
    {
      auto &task = Generator->getTask();
      process(task, TUPLE_COUNT);
      Generator->markTaskDone(task);
    }
  }

  void process(const Task &task, const int len)
  {
    auto input = reinterpret_cast<const InputSchema_128 *>(task.data->data());
    for (int i = 0; i < len; ++i)
    {
      if (input[i].event_type != 0)
      {
        continue;
      }
      long campaignId = 0;
      auto foundId = StaticJoinTable->get_value(input[i].ad_id, campaignId);
      if (!foundId)
      {
        continue;
      }
      CounterVal curVal{1, input[i].timestamp};
      CurrentCountMap->insert_or_modify(campaignId, curVal, 0);
    }
    output(task);
  }

  void output(const Task &task)
  {
    resultThread.AddResult(task, CurrentCountMap, &TablePool);
    CurrentCountMap = TablePool.acquire();
  }
};
void parse_args(int argc, const char **argv)
{
  namespace po = boost::program_options;
  po::options_description desc("Allowed options");
  desc.add_options()("help", "produce help message")("thread-count", po::value<int>(), "")("run-length", po::value<int>(), "seconds before exiting");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help"))
  {
    std::cout << desc << "\n";
    std::exit(1);
  }

  if (vm.count("thread-count"))
  {
    ThreadCount = vm["thread-count"].as<int>();
  }
  std::cout << "Number of threads set to "
            << ThreadCount << ".\n";
  if (vm.count("run-length"))
  {
    RunLength = vm["run-length"].as<int>();
    std::cout << "Running experiment for "
              << RunLength << ".\n";
  }
}
int main(int argc, const char **argv)
{
  numa_set_bind_policy(1);
  numa_set_preferred(NUMA_NODE);
  parse_args(argc, argv);
  std::vector<std::shared_ptr<WorkerThread>> workers;
  StaticJoinData = generateStaticData();
  boost::thread_group generatorThreads;
  auto generator = std::make_shared<GeneratorThread>();
  auto generatorThread = generatorThreads.create_thread(
      boost::bind(&GeneratorThread::runGenerator, generator));
  pthread_setname_np(generatorThread->native_handle(), "Generator");
  boost::thread_group workerThreads;
  for (unsigned int i = 0; i < ThreadCount; ++i) {
    auto worker = std::make_shared<WorkerThread>(i, generator);
    workers.push_back(worker);
    auto thread = workerThreads.create_thread(
        boost::bind(&WorkerThread::RunWorker, worker));
    pthread_setname_np(thread->native_handle(), "Worker");
  }

  auto reportTime = RunLength <= 0 ? 2 : RunLength;
  boost::this_thread::sleep_for(boost::chrono::seconds(2));
  DataCounter.exchange(0, boost::memory_order_relaxed);
  while (true)
  {
    boost::this_thread::sleep_for(boost::chrono::seconds(reportTime));
    long throughput = DataCounter.exchange(0, boost::memory_order_relaxed);
    std::cout << "throughput: " << throughput / reportTime << std::endl;
    if (RunLength > 0)
    {
      exit(0);
    }
  }

  return 0;
}