#include "LatencyMonitor.h"

std::vector<double> LatencyMonitor::measurements;
std::list<double> LatencyMonitor::rawMeasurements;
std::atomic_bool LatencyMonitor::active = true;

