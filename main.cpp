#include <iostream>
#include <random>
#include <unordered_set>
#include <iterator>
#include <fstream>
#include <sstream>
#include "HashTable.h"
#include <boost/thread.hpp>

bool debug = false;
#define TUPLE_COUNT 1024 * 64

struct InputSchema_128 {
    long timestamp;
    long padding_0;
    long user_id;
    long page_id;
    long ad_id;
    long ad_type;
    long event_type;
    long ip_address;
    long padding_1;
    long padding_2;
    long padding_3;
    long padding_4;
    long padding_5;
    long padding_6;
    long padding_7;
    long padding_8;

    static void parse(InputSchema_128 &tuple, std::string &line) {
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

std::vector<char> *generateStaticData() {
    std::random_device rd;
    std::mt19937_64 eng(rd());
    std::uniform_int_distribution<long> distr(0, 1000000);
    std::unordered_set<long> set;
    auto staticData = new std::vector<char>(2 * sizeof(long) * 1024);
    auto staticBuf = (long *) staticData->data();

    long campaign_id = distr(eng); //0;
    set.insert(campaign_id);
    for (unsigned long i = 0; i < 1000; ++i) {
        if (i > 0 && i % 10 == 0) {
            campaign_id = distr(eng); //++;
            bool is_in = set.find(campaign_id) != set.end();
            while (is_in) {
                campaign_id = distr(eng);
                is_in = set.find(campaign_id) != set.end();
            }
        }
        staticBuf[i * 2] = i;
        staticBuf[i * 2 + 1] = (long) campaign_id;
    }
    return staticData;
}

std::vector<char> *generateData(std::vector<char> *staticData) {
    auto staticBuf = (long *) staticData->data();
    std::random_device rd;
    std::mt19937_64 eng(rd());
    std::uniform_int_distribution<long> distr(0, 1000000);
    size_t len = sizeof(InputSchema_128) * TUPLE_COUNT;
    auto data = new std::vector<char>(len);
    auto buf = (InputSchema_128 *) data->data();

    std::string line;
    auto user_id = distr(eng);
    auto page_id = distr(eng);
    unsigned long idx = 0;
    while (idx < len / sizeof(InputSchema_128)) {
        auto ad_id = staticBuf[((idx % 100000) % 1000) * 2];
        auto ad_type = (idx % 100000) % 5;
        auto event_type = (idx % 100000) % 3;
        line = std::to_string(idx) + " " + std::to_string(user_id) + " " + std::to_string(page_id) + " " +
               std::to_string((long) ad_id) + " " + std::to_string(ad_type) + " " + std::to_string(event_type) + " " +
               std::to_string(-1);
        InputSchema_128::parse(buf[idx], line);
        idx++;
    }

    if (debug) {
        std::cout << "timestamp user_id page_id ad_id ad_type event_type ip_address" << std::endl;
        for (unsigned long i = 0; i < data->size() / sizeof(InputSchema_128); ++i) {
            printf("[DBG] %09lu: %7ld %13ld %8ld %13ld %3ld %6ld %2ld \n",
                   i, buf[i].timestamp, (long) buf[i].user_id, (long) buf[i].page_id, (long) buf[i].ad_id,
                   buf[i].ad_type, buf[i].event_type, (long) buf[i].ip_address);
        }
    }
    return data;
};


void updateTimeStamps(InputSchema_128 *input, int count, long offset) {
    for (int i = 0; i < count; ++i) {
        input[i].timestamp += offset;
    }
}

struct CounterVal {
    long _1;
    long _2;
};

std::vector<char> *StaticJoinData;
boost::atomic_int64_t DataCounter(0);
auto ThreadCount = 5;


std::shared_ptr<HashTable<long, long>> loadStaticJoinTable(int segment, int totalSegments) {
    auto *dataPtr = reinterpret_cast<long *>(StaticJoinData->data());
    auto table = std::make_shared<HashTable<long, long>>(1024);
    auto segmentSize = (1000 / totalSegments);
    printf("segment %d %d %d", segment, segment * segmentSize, (segment + 1) * segmentSize - 1);
    for (int i = segment * segmentSize; i < (segment + 1) * segmentSize; ++i) {
        table->insert(dataPtr[i * 2], dataPtr[i * 2 + 1], 0);
    }
    return table;
}

struct WorkerThread {
    long lastWindowId = 0;
    std::shared_ptr<HashTable<long, long>> StaticJoinTable;
    HashTable<long, HashTable<long, CounterVal> *> KeyMapper = HashTable<long, HashTable<long, CounterVal> *>(1024);
    std::vector<char> *GeneratedData;

    void RunWorker(int number) {
        std::cout << "thread " << number << " starting" << std::endl;
        StaticJoinTable = loadStaticJoinTable(number, ThreadCount);
        GeneratedData = generateData(StaticJoinData);
        auto *dataPtr = reinterpret_cast<InputSchema_128 *>(GeneratedData->data());
        while (true) {
            process(dataPtr, TUPLE_COUNT);
            updateTimeStamps(dataPtr, TUPLE_COUNT, TUPLE_COUNT);
        }
    }

    void process(const InputSchema_128 *input, const int len) {
        for (int i = 0; i < len; ++i) {
            long campaignId = 0;
            auto foundId = StaticJoinTable->get_value(input[i].ad_id, campaignId);
            if (!foundId) {
                continue;
            }
            auto window_id = input[i].timestamp / 10000;
            if (window_id > lastWindowId) {
                output();
                lastWindowId = window_id;
            }
            if (input[i].event_type != 0) {
                continue;
            }
            HashTable<long, CounterVal> *windows = nullptr;
            if (!KeyMapper.get_value(campaignId, windows)) {
                windows = new HashTable<long, CounterVal>(1024 * 16);
                KeyMapper.insert(campaignId, windows, input[i].timestamp);
            }
            CounterVal curVal{1, input[i].timestamp};
            windows->insert_or_modify(lastWindowId, curVal, 0);
        }
    }

    void output() {
        auto buckets = KeyMapper.getBuckets();
        auto processed = 0;
        for (unsigned int i = 0; i < KeyMapper.getNumberOfBuckets(); ++i) {
            auto bucket = &buckets[i];
            if (bucket->state) {
                CounterVal curVal;
                if (bucket->value->get_value(lastWindowId, curVal)) {
                    bucket->value->evict(lastWindowId);
                    processed++;
                }
            }
        }
        DataCounter.fetch_add(processed * sizeof(InputSchema_128), boost::memory_order_relaxed);
    }
};

int main() {
    std::vector<std::shared_ptr<WorkerThread>> workers;
    StaticJoinData = generateStaticData();
    boost::thread_group workerThreads;
    for (int i = 0; i < ThreadCount; ++i) {
        auto worker = std::make_shared<WorkerThread>();
        workers.push_back(worker);
        workerThreads.create_thread(boost::bind(&WorkerThread::RunWorker, worker, i));
    }
    while (true) {
        boost::this_thread::sleep_for(boost::chrono::seconds(2));
        long throughput = DataCounter.exchange(0, boost::memory_order_relaxed);
        std::cout << "throughput: " << throughput << std::endl;
    }
    return 0;
}

