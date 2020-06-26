#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <random>
#include <unordered_set>

struct InputSchema {
  long timestamp;
  long padding_0;
  long user_id;
  long page_id;
  long ad_id;
  long ad_type;
  long event_type;
  long ip_address;
};

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

void generateData(long node, long totalNodes, long step,
                  const std::vector<long> staticBuf, long campaignKeys,
                  long len) {
  std::ofstream file;
  file.open("input_" + std::to_string(node) + "_" + std::to_string(totalNodes) +
            ".dat",
            std::ios::out | std::ios::binary);

  std::random_device rd;
  std::mt19937_64 eng(rd());
  std::uniform_int_distribution<long> distr(0, 1000000);
  std::string line;
  InputSchema schema;
  auto user_id = distr(eng);
  auto page_id = distr(eng);
  for (long i = 0; i < len * totalNodes; i += step * totalNodes) {
    unsigned long idx = i + node * step;
    while (idx < i + (node + 1) * step) {
      auto ad_id = staticBuf[((idx % 100000) % campaignKeys) * 2];
      auto ad_type = (idx % 100000) % 5;
      auto event_type = (idx % 100000) % 3;
      schema.timestamp = (long)idx;
      schema.padding_0 = (long)0;
      schema.user_id = (long)user_id;
      schema.page_id = (long)page_id;
      schema.ad_id = (long)ad_id;
      schema.ad_type = (long)ad_type;
      schema.event_type = (long)event_type;
      schema.ip_address = (long)-1;
      idx++;
      file.write((char *)(&schema), sizeof(InputSchema));
    }
  }
  file.close();
}

int NodesUsed = 2;
int BatchElementCount = 1024 * 16;
int BatchCount = 1000;

void parse_args(int argc, const char **argv) {
  namespace po = boost::program_options;
  po::options_description desc("Allowed options");
  desc.add_options()("help", "produce help message")(
      "batch-size", po::value<int>(), "number of elements in a batch")(
      "batch-count", po::value<int>(), "number of batches generated per node")(
      "nodes", po::value<int>(), "NUMA nodes used");
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
  if (vm.count("batch-size")) {
    BatchElementCount = vm["batch-size"].as<int>();
  }
  std::cout << "Batch size set to " << BatchElementCount << ".\n";
  if (vm.count("batch-count")) {
    BatchCount = vm["batch-count"].as<int>();
  }
  std::cout << "Number of batches per node set to " << BatchCount << ".\n";
}

int main(int argc, const char **argv) {
  parse_args(argc, argv);
  auto staticData = generateStaticData();
  std::ofstream file("input_" + std::to_string(NodesUsed) + ".dat",
                     std::ifstream::binary);
  unsigned int len = staticData.size();
  file.write((char *)&len, sizeof(len));
  file.write((const char *)&staticData[0], len * sizeof(long));
  file.close();
  for (int i = 0; i < NodesUsed; ++i) {
    generateData(i, NodesUsed, BatchElementCount, staticData, 1000,
                 BatchCount * BatchElementCount);
  }
}