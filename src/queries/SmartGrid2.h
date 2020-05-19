#ifndef PROOFOFCONCEPT_SMARTGRID2_H
#define PROOFOFCONCEPT_SMARTGRID2_H

#include "../HashFunctions.h"
#include "../HashTable.h"
#include "../HashTablePool.h"
#include "../QueryTask.h"
#include "../TaskResult.h"
#include <atomic>
struct Key {
  int plug;
  int household;
  int house;

  bool operator==(const Key &rhs) const {
    return plug == rhs.plug && household == rhs.household && house == rhs.house;
  }
};

namespace std {
template <> struct hash<Key> {
  inline size_t operator()(const Key &k) const {
    // size_t value = your hash computations over x
    return Crc32Hash<Key, sizeof(Key)>{}(k);
  }
};
} // namespace std

class SmartGrid2 {
public:
  using KeyType = Key;

  struct CounterVal {
    long _1;
    long _2;
  };
  using LocalValueType = CounterVal;
  using GlobalValueType = std::atomic<int64_t>;
  using TResult = TaskResult<KeyType>;
  struct InputSchema {
    long timestamp;
    float value;
    int property;
    int plug;
    int household;
    int house;
  };
private:
  static long DataRange;
  HashTablePool<KeyType, CounterVal> TablePool;
  std::function<void(TResult &&)> OutputCb;

public:
  explicit SmartGrid2(std::vector<char> &staticJoinData);
  void process(const QueryTask &task);
  void SetOutputCb(std::function<void(TResult &&)> outputCb);
  static void merge(TResult &a, const TResult &b);
  static long ComputeTimestampOffset(int batch);
  static void* loadData(int node, int totalNodes);
  static std::vector<char> loadStaticData(int totalNodes);
};

#endif // PROOFOFCONCEPT_SMARTGRID2_H
