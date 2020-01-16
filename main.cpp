#include <iostream>
#include <random>
#include <unordered_set>
#include <iterator>
#include <fstream>
#include <sstream>
#include "HashTable.h"
#include <boost/thread.hpp>

bool debug = false;
#define TUPLE_COUNT 16024

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

std::vector<char> *data = nullptr;
std::vector<char> *staticData = nullptr;

void loadInMemoryData_128() {
    std::random_device rd;
    std::mt19937_64 eng(rd());
    std::uniform_int_distribution<long> distr(0, 1000000);

    std::unordered_set<long> set;

    size_t len = sizeof(InputSchema_128) * TUPLE_COUNT;
    data = new std::vector<char>(len);
    staticData = new std::vector<char>(2 * sizeof(long) * 1024);
    auto buf = (InputSchema_128 *) data->data();
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
};

struct CounterVal {
    long _1;
    long _2;
};

HashTable<long, HashTable<long, CounterVal> *> KeyMapper(1024);
HashTable<long, long> StaticJoinTable(1024);

void loadStaticJoinTable() {
    auto *dataPtr = reinterpret_cast<long *>(staticData->data());
    for (int i = 0; i < 1000; ++i) {
        StaticJoinTable.insert(dataPtr[i * 2], dataPtr[i * 2 + 1], 0);
    }
}

void output(const long lastWindowId) {
    auto buckets = KeyMapper.getBuckets();
    std::cout << "output for window " << lastWindowId;
    for (int i = 0; i < KeyMapper.getNumberOfBuckets(); ++i) {
        if (buckets[i].state) {
            std::cout << " k: " << buckets[i].key << " c: ";
            CounterVal curVal;
            if (buckets[i].value->get_value(lastWindowId, curVal)) {
                std::cout << curVal._1;
            }
        }
    }
    std::cout << std::endl;
}

long lastWindowId = 0;

void process(const InputSchema_128 *input, const int len) {
    for (int i = 0; i < len; ++i) {
        auto window_id = input[i].timestamp / 1000;
        if (window_id > lastWindowId) {
            output(lastWindowId);
            lastWindowId = window_id;
        }
        if (input[i].event_type != 0) {
            continue;
        }
        HashTable<long, CounterVal> *windows = nullptr;
        long campaignId = 0;
        auto foundId = StaticJoinTable.get_value(input[i].ad_id, campaignId);
        if (!foundId) {
            std::cout << "Failed to static join!" << std::endl;
        }
        if (!KeyMapper.get_value(campaignId, windows)) {
            windows = new HashTable<long, CounterVal>(1024);
            KeyMapper.insert(campaignId, windows, input[i].timestamp);
        }
        CounterVal curVal;
        curVal._1 = 1;
        curVal._2 = input[i].timestamp;
        windows->insert_or_modify(lastWindowId, curVal, 0);
    }
}

struct WorkerThread
{
    void RunWorker(int number)
    {
        std::cout << "thread number: " << number << std::endl;
    }
};

int main() {
    auto ThreadCount = 10;
    std::vector<std::shared_ptr<WorkerThread>> workers;
    boost::thread_group workerThreads;
    for (int i = 0; i < ThreadCount; ++i) {
        auto worker = std::make_shared<WorkerThread>();
        workers.push_back(worker);
        workerThreads.create_thread(boost::bind(&WorkerThread::RunWorker, worker, i));
    }
    loadInMemoryData_128();
    loadStaticJoinTable();
    auto *dataPtr = reinterpret_cast<InputSchema_128 *>(data->data());
    process(dataPtr, TUPLE_COUNT);
    return 0;
}
