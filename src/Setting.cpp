#include "Setting.h"
long Setting::PAGE_SIZE = sysconf(_SC_PAGE_SIZE);
int Setting::NODES_USED = -1;
boost::atomic_int64_t Setting::DataCounter(0);