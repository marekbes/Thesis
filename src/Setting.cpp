#include "Setting.h"
long Setting::BATCH_COUNT = 1024 * 16;
long Setting::BATCH_SIZE = 0;
long Setting::PAGE_SIZE = sysconf(_SC_PAGE_SIZE);
long Setting::DATA_COUNT = 0;
int Setting::NODES_USED = -1;
int Setting::THREADS_USED = -1;
boost::atomic_int64_t Setting::DataCounter(0);
std::string Setting::DATA_PATH = "/mnt/ssd/mb4416/";
int Setting::WINDOW_SLIDE = 20000;
int Setting::WINDOW_SIZE = 100000;
int Setting::SHARED_SLOTS = 10000;
int Setting::AGGREGATE_KEYS = 256;