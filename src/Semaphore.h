//
// Created by mb4416 on 30/01/2020.
//

#ifndef PROOFOFCONCEPT_SEMAPHORE_H
#define PROOFOFCONCEPT_SEMAPHORE_H

#include <condition_variable>
#include <mutex>

class semaphore {
private:
  std::mutex mutex_;
  std::condition_variable condition_;
  unsigned long count_ = 0; // Initialized as locked.

public:
  semaphore(int count = 0) : count_(count) {}

  void up() {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    ++count_;
    condition_.notify_one();
  }

  void down() {
    std::unique_lock<decltype(mutex_)> lock(mutex_);
    while (!count_) // Handle spurious wake-ups.
      condition_.wait(lock);
    --count_;
  }

  bool try_down() {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    if (count_) {
      --count_;
      return true;
    }
    return false;
  }
};

#endif // PROOFOFCONCEPT_SEMAPHORE_H
