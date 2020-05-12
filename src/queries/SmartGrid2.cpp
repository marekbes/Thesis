#include "SmartGrid2.h"
#include "../Setting.h"
#include <fstream>
#include <numa.h>

void SmartGrid2::process(const QueryTask &task) {
  bool startingWindow = false;
  auto countMap = TablePool.acquire();

  auto input = reinterpret_cast<const InputSchema *>(task.data);
  auto lastWindowId = (input[0].timestamp + task.timestampOffset);
#ifdef POC_DEBUG_POSITION
  auto lastOutputPos = task.startPos;
#endif
  for (int i = 0; i < task.size; ++i) {
    auto &elem = input[i];
    auto windowId = (elem.timestamp + task.timestampOffset);
    if (windowId != lastWindowId) {
#ifdef POC_DEBUG_POSITION
      this->OutputCb(TResult{lastWindowId, startingWindow, true, 0,
                             std::move(countMap), task.batchId, lastOutputPos,
                             task.startPos + i - 1});
      lastOutputPos = task.startPos + i;
#else
      this->OutputCb(TResult{lastWindowId, startingWindow, true, 0,
                             std::move(countMap), task.batchId});
#endif
      startingWindow = true;
      countMap = this->TablePool.acquire();
#ifdef POC_DEBUG
      std::stringstream stream;
      stream << "Map " << countMap.get() << "for batch " << std::dec
             << task.batchId << std::endl;
      std::cout << stream.str();
#endif
      lastWindowId = windowId;
    }
    countMap->insert_or_modify(Key{elem.plug, elem.household, elem.house},
                               CounterVal{1, elem.timestamp}, windowId);
  }
  this->OutputCb(TResult{lastWindowId, startingWindow, false, 0,
                         std::move(countMap), task.batchId
#ifdef POC_DEBUG_POSITION
                         ,
                         lastOutputPos, task.endPos
#endif
  });
}
void SmartGrid2::SetOutputCb(std::function<void(TResult &&)> outputCb) {
  this->OutputCb = std::move(outputCb);
}
void SmartGrid2::merge(TResult &a, const TResult &b) {}

std::vector<char> SmartGrid2::loadStaticData(int) {
  return std::vector<char>();
}

static void parse(SmartGrid2::InputSchema &tuple, std::string &line,
                  long &normalisedTimestamp) {
  std::istringstream iss(line);
  std::vector<std::string> words{std::istream_iterator<std::string>{iss},
                                 std::istream_iterator<std::string>{}};
  if (normalisedTimestamp == -1)
    normalisedTimestamp = std::stol(words[0]);

  tuple.timestamp = std::stol(words[0]) - normalisedTimestamp;
  tuple.value = std::stof(words[1]);
  tuple.property = std::stoi(words[2]);
  tuple.plug = std::stoi(words[3]);
  tuple.household = std::stoi(words[4]);
  tuple.house = std::stoi(words[5]);
}

void *SmartGrid2::loadData(int node, int totalNodes) {
  auto dataCount = 33627;
  assert(dataCount > totalNodes * Setting::BATCH_COUNT);
  Setting::DATA_COUNT = 1;
  long len = Setting::DATA_COUNT * Setting::BATCH_COUNT;
  auto data = numa_alloc_onnode(Setting::DATA_COUNT * Setting::BATCH_SIZE, node);
  auto buf = (InputSchema *)data;

  std::ifstream file("datasets/smartgrid-data.txt");
  std::string line;
  long idx = 0;
  long normalisedTimestamp = -1;
  while (std::getline(file, line) && idx < len) {
    parse(buf[idx], line, normalisedTimestamp);
    idx++;
  }
  DataRange = buf[idx - 1].timestamp - buf[0].timestamp;
  for (int i = 0; i < idx; i++) {
    buf[i].timestamp += node * DataRange;
  }
  file.close();
  return data;
}

SmartGrid2::SmartGrid2(std::vector<char> &) {}
long SmartGrid2::ComputeTimestampOffset(int batch)
{
  return batch * DataRange * Setting::NODES_USED;
}
long SmartGrid2::DataRange = 0;
