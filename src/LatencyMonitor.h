#ifndef PROOFOFCONCEPT_LATENCYMONITOR_H
#define PROOFOFCONCEPT_LATENCYMONITOR_H
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <vector>

class LatencyMonitor {
public:
  using Timestamp_t = std::chrono::high_resolution_clock::time_point;

private:
  static std::atomic_bool active;
  static std::list<double> rawMeasurements;
  static std::vector<double> measurements;

public:
  static inline Timestamp_t GetTimestamp() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    return currentTime;
  }
  static inline void InsertMeasurement(Timestamp_t t) {
    if (!(active.load()))
      return;
    auto dt = std::chrono::duration<double>(GetTimestamp() - t).count();
    rawMeasurements.push_back(dt);
  }

  static void PrintStatistics() {
    active = false;
      std::copy(rawMeasurements.begin(), rawMeasurements.end(),
                std::back_inserter(measurements));
    int length = measurements.size();

    //    std::cout << "[MON] [LatencyMonitor] " << std::to_string(length)
    //              << " measurements" << std::endl;

    if (length < 1)
      return;
    std::sort(measurements.begin(), measurements.end());

    std::stringstream streamObj;
    streamObj << std::fixed;
    streamObj << std::setprecision(3);
    streamObj << "[MON] [LatencyMonitor] 5th "
              << std::to_string(evaluateSorted(5));
    streamObj << " 25th " << std::to_string(evaluateSorted(25));
    streamObj << " 50th " << std::to_string(evaluateSorted(50));
    streamObj << " 75th " << std::to_string(evaluateSorted(75));
    streamObj << " 99th " << std::to_string(evaluateSorted(99));
    std::cout << streamObj.str() << std::endl;
  }

private:
  static double evaluateSorted(const double p) {
    double n = measurements.size();
    double pos = p * (n + 1) / 100;
    double fpos = floor(pos);
    int intPos = (int)fpos;
    double dif = pos - fpos;

    if (pos < 1) {
      return measurements[0];
    }
    if (pos >= n) {
      return measurements.back();
    }
    double lower = measurements[intPos - 1];
    double upper = measurements[intPos];
    return lower + dif * (upper - lower);
  }
};

#endif // PROOFOFCONCEPT_LATENCYMONITOR_H
