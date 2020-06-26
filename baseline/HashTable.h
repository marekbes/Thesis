#pragma once

#include <climits>
#include <iterator>
#include <utility>
#include <bits/stdc++.h>
//#include <cstring>

#define MAP_SIZE 256
#define KEY_SIZE 4
#define VALUE_SIZE 4

template <typename T> struct HashMapEqualTo {
  constexpr bool operator()(const T &lhs, const T &rhs) const {
    return lhs == rhs;
  }
};

template <class KeyT, class ValueT> struct alignas(16) Bucket {
  char state;
  char dirty;
  long timestamp;
  KeyT key;
  ValueT value;
  int counter;
};

template <class ValueT> struct DummyAggr {
  unsigned int addedElements = 0;
  unsigned int removedElements = 0;
  void initialise(){

  };
  void insert(ValueT v){

  };
  ValueT query() { return 0; };
  void evict(){

  };
};

template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>,
          typename EqT = HashMapEqualTo<KeyT>,
          typename AggrT = DummyAggr<ValueT>>
class alignas(64) HashTable {
private:
  using BucketT = Bucket<KeyT, ValueT>;

  HashT _hasher;
  EqT _eq;
  BucketT *_buckets = nullptr;
  AggrT *_aggrs = nullptr;
  size_t _num_buckets = 0;
  size_t _num_filled = 0;
  size_t _mask = 0;

  // int*      _reset_cnts   = nullptr;
public:
  HashTable() : HashTable(256){};

  HashTable(size_t size) : _num_buckets(size), _mask(size - 1) {
    if (!(_num_buckets && !(_num_buckets & (_num_buckets - 1)))) {
      throw std::runtime_error(
          "error: the size of the hash table has to be a power of two\n");
    }

    _buckets = (BucketT *)malloc(_num_buckets * sizeof(BucketT));
    _aggrs = (AggrT *)malloc(_num_buckets * sizeof(AggrT));
    //_reset_cnts = (int *)malloc(_num_buckets * sizeof(int));
    if (!_buckets || !_aggrs) {
      free(_buckets);
      free(_aggrs);
      throw std::bad_alloc();
    }

    for (unsigned int i = 0; i < _num_buckets; ++i) {
      _buckets[i].state = 0;
      _buckets[i].dirty = 0;
      //_reset_cnts[i] = 0;
      //_aggrs[i] = AggrT (); // maybe initiliaze this on insert
      //_aggrs[i].initialise();
    }
  }

  void reset() {
    for (auto i = 0; i < _num_buckets; ++i) {
      _buckets[i].state = 0;
      //_buckets[i].dirty = 0;
      //_aggrs[i].initialise();
      /*if (_reset_cnts[i] == 30) {
          _buckets[i].dirty = 0;
          _aggrs[i].initialise();
      }*/
    }
    _num_filled = 0;
  }

  void clear() {
    for (unsigned int i = 0; i < _num_buckets; ++i) {
      _buckets[i].state = 0;
      _buckets[i].dirty = 0;
      _aggrs[i].initialise();
    }
    _num_filled = 0;
  }

  void insert(const KeyT &key, const ValueT &value, const long timestamp) {
    size_t ind = _hasher(key) & _mask, i = ind;
    for (; i < _num_buckets; i++) {
      if (!_buckets[i].state || _buckets[i].key == key) {
        _buckets[i].state = 1;
        _buckets[i].timestamp = timestamp;
        _buckets[i].key = key; // std::memcpy(&_buckets[i].key, key, KEY_SIZE);
        _buckets[i].value = value;
        _num_filled++;
        return;
      }
    }
    for (i = 0; i < ind; i++) {
      if (!_buckets[i].state || _buckets[i].key == key) {
        _buckets[i].state = 1;
        _buckets[i].timestamp = timestamp;
        _buckets[i].key = key;
        _buckets[i].value = value;
        _num_filled++;
        return;
      }
    }
    throw std::runtime_error("error: the hashtable is full \n");
  }

  void insert_or_modify(const KeyT &key, const ValueT &value, long timestamp) {
    size_t ind = _hasher(key) & _mask, i = ind;
    char tempState;
    for (; i < _num_buckets; i++) {
      tempState = _buckets[i].state;
      if (tempState && _eq(_buckets[i].key, key)) {
        _buckets[i].value._1 = _buckets[i].value._1 + value._1;
        _buckets[i].counter++;
        return;
      }
      if (!tempState &&
          (_eq(_buckets[i].key, key) ||
           _buckets[i].dirty ==
               0)) { // first insert -- keep track of previous inserted value
        _buckets[i].state = 1;
        _buckets[i].dirty = 1;
        _buckets[i].timestamp = timestamp;
        _buckets[i].key = key;
        _buckets[i].value = value;
        _buckets[i].counter = 1;
        _num_filled++;
        return;
      }
    }
    for (i = 0; i < ind; i++) {
      tempState = _buckets[i].state;
      if (tempState && _eq(_buckets[i].key, key)) {
        _buckets[i].value._1 =
            _buckets[i].value._1 < value._1 ? _buckets[i].value._1 : value._1;
        _buckets[i].counter++;
        return;
      }
      if (!tempState &&
          (_eq(_buckets[i].key, key) ||
           _buckets[i].dirty ==
               0)) { // first insert -- keep track of previous inserted value
        _buckets[i].state = 1;
        _buckets[i].dirty = 1;
        _buckets[i].timestamp = timestamp;
        _buckets[i].key = key;
        _buckets[i].value = value;
        _buckets[i].counter = 1;
        _num_filled++;
        return;
      }
    }

    /*for (int i = 0; i < _num_buckets; ++i) {
            printf("%d: state %d dirty %d, %d %d %d %d\n", i, _buckets[i].state,
    _buckets[i].dirty, _buckets[i].key._0, _buckets[i].key._1,
    _buckets[i].key._2, _buckets[i].key._3);
    }*/

    throw std::runtime_error("error: the hashtable is full \n");
  }

  bool evict(const KeyT &key) {
    size_t ind = _hasher(key) & _mask, i = ind;
    for (; i < _num_buckets; i++) {
      if (_buckets[i].state && _eq(_buckets[i].key, key)) {
        _buckets[i].dirty = 0;
        _buckets[i].state = 0;
        _num_filled--;
        return true;
      }
    }
    for (i = 0; i < ind; i++) {
      if (_buckets[i].state && _eq(_buckets[i].key, key)) {
        _buckets[i].dirty = 0;
        _buckets[i].state = 0;
        _num_filled--;
        return true;
      }
    }
    printf("error: entry not found \n");
    return false;
  }

  void insertSlices() {
    int maxNumOfSlices = INT_MIN;
    for (auto i = 0; i < _num_buckets; ++i) {
      int temp = _aggrs[i].addedElements - _aggrs[i].removedElements;
      if (temp > 0)
        maxNumOfSlices = maxNumOfSlices < temp ? maxNumOfSlices : temp;
    }
    for (auto i = 0; i < _num_buckets; ++i) {
      int temp = _aggrs[i].addedElements - _aggrs[i].removedElements;
      if (_buckets[i].state) {
        //_reset_cnts[i] = 0;
        if (maxNumOfSlices != INT_MIN && temp < maxNumOfSlices) {
          for (int j = temp; j < maxNumOfSlices; ++j) {
            ValueT val;
            typename AggrT::node n;
            n._1 = val._1;
            n._c1 = 0;
            // n._2 = val._2;
            _aggrs[i].insert(n);
          }
        } else {
          typename AggrT::node n;
          n._1 = _buckets[i].value._1;
          n._c1 = _buckets[i].counter;
          // n._2 = _buckets[i].value._2;
          _aggrs[i].insert(n);
        }
        _buckets[i].state = 0;
        //_buckets[i].value = ValueT();
      } else if (temp > 0) {
        //_reset_cnts[i]++;
        ValueT val;
        typename AggrT::node n;
        n._1 = val._1;
        // n._2 = val._2;
        _aggrs[i].insert(n);
      }
    }
  }

  void evictSlices() {
    for (auto i = 0; i < _num_buckets; ++i) {
      if (_aggrs[i].addedElements - _aggrs[i].removedElements > 0) {
        _aggrs[i].evict();
      }
    }
  }

  void setValues() {
    for (auto i = 0; i < _num_buckets; ++i) {
      if (_aggrs[i].addedElements - _aggrs[i].removedElements > 0) {
        auto res = _aggrs[i].query();
        _buckets[i].state = 1;
        _buckets[i].value._1 = res._1;
        //_buckets[i].value._2 = res._2;
        _buckets[i].counter = 1;
      }
    }
  }

  void setIntermValues(int pos) {
    for (auto i = 0; i < _num_buckets; ++i) {
      if (_aggrs[i].addedElements - _aggrs[i].removedElements > 0) {
        auto res = _aggrs[i].queryIntermediate(pos);
        _buckets[i].state = 1;
        _buckets[i].value._1 = res._1;
        //_buckets[i].value._2 = res._2;
        _buckets[i].counter = res._c1;
      }
    }
  }

  bool get_value(const KeyT &key, ValueT &result) {
    size_t ind = _hasher(key) & _mask, i = ind;
    for (; i < _num_buckets; i++) {
      if (_buckets[i].state && _eq(_buckets[i].key, key)) {
        result = _buckets[i].value;
        return true;
      }
    }
    for (i = 0; i < ind; i++) {
      if (_buckets[i].state && _eq(_buckets[i].key, key)) {
        result = _buckets[i].value;
        return true;
      }
    }
    return false;
  }

  bool get_result(const KeyT &key, ValueT &result) {
    size_t ind = _hasher(key) & _mask, i = ind;
    for (; i < _num_buckets; i++) {
      if (_buckets[i].state && _eq(_buckets[i].key, key)) {
        _buckets[i].res = _aggrs->query();
        result = _buckets[i].res;
        return true;
      }
    }
    for (i = 0; i < ind; i++) {
      if (_buckets[i].state && _eq(_buckets[i].key, key)) {
        _buckets[i].res = _aggrs->query();
        result = _buckets[i].res;
        return true;
      }
    }
    return false;
  }

  bool get_index(const KeyT &key, size_t &index) {
    size_t ind = _hasher(key) & _mask, i = ind;
    int dist = 0;
    for (; i < _num_buckets; i++) {
      if (_buckets[i].state && _eq(_buckets[i].key, key)) {
        index = i;
        return true;
      }
      // dist++;
      // if (dist==1)
      //    return false;
      /*if (!_buckets[i].state) {
          index = i;
          return false;
      }*/
    }
    for (i = 0; i < ind; i++) {
      if (_buckets[i].state && _eq(_buckets[i].key, key)) {
        index = i;
        return true;
      }
      // dist++;
      // if (dist==1)
      //    return false;
      /*if (!_buckets[i].state) {
          index = i;
          return false;
      }*/
    }
    return false;
  }

  void deleteHashTable() {
    for (size_t bucket = 0; bucket < _num_buckets; ++bucket) {
      _buckets[bucket].~BucketT();
      _aggrs->~AggrT();
    }
    free(_buckets);
    free(_aggrs);
  }

  BucketT *getBuckets() { return _buckets; }

  size_t getSize() const { return _num_filled; }

  bool isEmpty() const { return _num_filled == 0; }

  size_t getNumberOfBuckets() const { return _num_buckets; }

  float load_factor() const {
    return static_cast<float>(_num_filled) / static_cast<float>(_num_buckets);
  }

  void merge(HashTable &other) {
    auto *buckets = other.getBuckets();
    for (size_t i = 0; i < other.getNumberOfBuckets(); ++i) {
      if (buckets[i].state) {
        insert_or_modify(buckets[i].key, buckets[i].value,
                         buckets[i].timestamp);
      }
    }
  }
};