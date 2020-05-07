#pragma once

/*
 * \brief Different hash functions that can be used for code generation.
 *
 * */

#include <iterator>
#include <x86intrin.h>

#define CRCPOLY 0x82f63b78 // reversed 0x1EDC6F41
#define CRCINIT 0xFFFFFFFF
//static uint32_t Crc32Lookup[8][256];

extern uint32_t Crc32Lookup[256];

template<typename T, size_t len>
struct Crc32Hash {
  /*static void Crc32LookupInit() {
      for (uint32_t i = 0; i <= 0xFF; i++) {
          uint32_t x = i;
          for (uint32_t j = 0; j < 8; j++)
              x = (x>>1) ^ (CRCPOLY & (-(int)(x & 1)));
          Crc32Lookup[0][i] = x;
      }

      for (uint32_t i = 0; i <= 0xFF; i++) {
          uint32_t c = Crc32Lookup[0][i];
          for (uint32_t j = 1; j < 8; j++) {
              c = Crc32Lookup[0][c & 0xFF] ^ (c >> 8);
              Crc32Lookup[j][i] = c;
          }
      }
  }*/
  // Hardware-accelerated CRC-32C (using CRC32 instruction)
  static inline size_t CRC_Hardware(const void *data, size_t length) {
    size_t crc = CRCINIT;

    unsigned char *current = (unsigned char *) data;
    // Align to DWORD boundary
    size_t align = (sizeof(unsigned int) - (__int64_t) current) & (sizeof(unsigned int) - 1);
    align = std::min(align, length);
    length -= align;
    for (; align; align--)
      crc = Crc32Lookup[(crc ^ *current++) & 0xFF] ^ (crc >> 8);

    size_t ndwords = length / sizeof(unsigned int);
    for (; ndwords; ndwords--) {
      crc = _mm_crc32_u32(crc, *(unsigned int *) current);
      current += sizeof(unsigned int);
    }

    length &= sizeof(unsigned int) - 1;
    for (; length; length--)
      crc = _mm_crc32_u8(crc, *current++);
    return ~crc;
  }
  Crc32Hash() {/*Crc32LookupInit();*/}
  std::size_t operator()(T t) const {
    return CRC_Hardware(&t, len);
  }
};

#if COMPILER == GCCC
#define FALLTHROUGH [[gnu::fallthrough]];
#elif  COMPILER == CLANG
#define FALLTHROUGH
#endif
template<class T, int len>
class CRC32Hash {
public:
  static const uint64_t seed = 902850234;
  inline auto hashKey(uint64_t k) const { // -> typename std::enable_if<IS_INT_LE(64,T), hash_t>::type{
    uint64_t result1 = _mm_crc32_u64(seed, k);
    uint64_t result2 = _mm_crc32_u64(0x04c11db7, k);
    return ((result2 << 32) | result1) * 0x2545F4914F6CDD1Dull;
  }
  //inline uint64_t hashKey(uint64_t k) const { return hashKey(k, 0); }
  inline uint64_t hashKey(const void *key, int length) const {
    auto data = reinterpret_cast<const uint8_t *>(key);
    uint64_t s = seed;
    while (length >= 8) {
      s = hashKey(*reinterpret_cast<const uint64_t *>(data));
      data += 8;
      length -= 8;
    }
    if (length >= 4) {
      s = hashKey((uint32_t) *reinterpret_cast<const uint32_t *>(data));
      data += 4;
      length -= 4;
    }
    switch (length) {
    case 3: s ^= ((uint64_t) data[2]) << 16;FALLTHROUGH
    case 2: s ^= ((uint64_t) data[1]) << 8;FALLTHROUGH
    case 1: s ^= data[0];FALLTHROUGH
    }
    return s;
  }
  std::size_t operator()(T t) const {
    return hashKey(&t, len);
  }
};

template<typename T, size_t len>
struct MurmurHash {
  static inline uint64_t MurmurHash64(void *key, int length, unsigned int seed) {
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;
    uint64_t h = seed ^(length * m);
    const uint64_t *data = (const uint64_t *) key;
    const uint64_t *end = data + (length / 8);
    while (data != end) {
      uint64_t k = *data++;
      k *= m;
      k ^= k >> r;
      k *= m;

      h ^= k;
      h *= m;
    }

    const unsigned char *data2 = (const unsigned char *) data;
    switch (length & 7) {
    case 7: h ^= uint64_t(data2[6]) << 48;
    case 6: h ^= uint64_t(data2[5]) << 40;
    case 5: h ^= uint64_t(data2[4]) << 32;
    case 4: h ^= uint64_t(data2[3]) << 24;
    case 3: h ^= uint64_t(data2[2]) << 16;
    case 2: h ^= uint64_t(data2[1]) << 8;
    case 1: h ^= uint64_t(data2[0]);
      h *= m;
    };
    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
  }
  std::size_t operator()(T t) const {
    return MurmurHash64(&t, len, 0);
  }
};

template<typename T, size_t len>
struct PythonHash {
#define _PyHASH_MULTIPLIER 1000003UL  /* 0xf4243 */
  static size_t pythonHash(const void *data, size_t length) {
    size_t x;  /* Unsigned for defined overflow behavior. */
    size_t y;
    size_t mult = _PyHASH_MULTIPLIER;
    x = 0x345678UL;
    unsigned char *current = (unsigned char *) data;
    auto hasher = std::hash<char>();
    while (--length >= 0) {
      y = hasher(*current++);
      if (y == -1)
        return -1;
      x = (x ^ y) * mult;
      /* the cast might truncate len; that doesn't change hash stability */
      mult += (size_t) (82520UL + length + length);
    }
    x += 97531UL;
    if (x == (size_t) -1)
      x = -2;
    return x;
  }
  std::size_t operator()(T t) const {
    return pythonHash(&t, len);
  }
};

template<typename T, size_t len>
struct JHash {

  static inline uint32_t rol32(uint32_t word, unsigned int shift) {
    return (word << shift) | (word >> (32 - shift));
  }

/* Best hash sizes are of power of two */
#define jhash_size(n)   ((uint32_t)1<<(n))
/* Mask the hash value, i.e (value & jhash_mask(n)) instead of (value % n) */
#define jhash_mask(n)   (jhash_size(n)-1)

/* __jhash_mix -- mix 3 32-bit values reversibly. */
#define __jhash_mix(a, b, c)            \
{                        \
    a -= c;  a ^= rol32(c, 4);  c += b;    \
    b -= a;  b ^= rol32(a, 6);  a += c;    \
    c -= b;  c ^= rol32(b, 8);  b += a;    \
    a -= c;  a ^= rol32(c, 16); c += b;    \
    b -= a;  b ^= rol32(a, 19); a += c;    \
    c -= b;  c ^= rol32(b, 4);  b += a;    \
}

/* __jhash_final - final mixing of 3 32-bit values (a,b,c) into c */
#define __jhash_final(a, b, c)            \
{                        \
    c ^= b; c -= rol32(b, 14);        \
    a ^= c; a -= rol32(c, 11);        \
    b ^= a; b -= rol32(a, 25);        \
    c ^= b; c -= rol32(b, 16);        \
    a ^= c; a -= rol32(c, 4);        \
    b ^= a; b -= rol32(a, 14);        \
    c ^= b; c -= rol32(b, 24);        \
}

/*
 * Arbitrary initial parameters
 */
#define JHASH_INITVAL    0xdeadbeef
#define LISTEN_SEED    0xfaceb00c
#define WORKER_SEED    0xb00cface

/* jhash2 - hash an array of uint32_t's
 * @k: the key which must be an array of uint32_t's
 * @length: the number of uint32_t's in the key
 * @initval: the previous hash, or an arbitray value
 *
 * Returns the hash value of the key.
 */
  static inline __attribute__((pure)) uint32_t jhash2(uint32_t *k,
                                                      uint32_t length, uint32_t initval) {
    uint32_t a, b, c;

    /* Set up the internal state */
    a = b = c = JHASH_INITVAL + (length << 2) + initval;

    /* Handle most of the key */
    while (length > 3) {
      a += k[0];
      b += k[1];
      c += k[2];
      __jhash_mix(a, b, c);
      length -= 3;
      k += 3;
    }

    /* Handle the last 3 uint32_t's: all the case statements fall through */
    switch (length) {
    case 3: c += k[2];
    case 2: b += k[1];
    case 1: a += k[0];
      __jhash_final(a, b, c);
    case 0:    /* Nothing left to add */
      break;
    }

    return c;
  }

  std::size_t operator()(T t) const {
    return jhash2((uint32_t *) &t, len / 4, 0);
  }
};

template<typename T, size_t len>
struct MurmurHash3 {
#if defined(_MSC_VER)

  #define FORCE_INLINE	__forceinline

#include <stdlib.h>

#define ROTL32(x,y)	_rotl(x,y)
#define ROTL64(x,y)	_rotl64(x,y)

#define BIG_CONSTANT(x) (x)

  // Other compilers

#else	// defined(_MSC_VER)

#define    FORCE_INLINE inline __attribute__((always_inline))

  inline uint32_t rotl32(uint32_t x, int8_t r) {
    return (x << r) | (x >> (32 - r));
  }

  inline uint64_t rotl64(uint64_t x, int8_t r) {
    return (x << r) | (x >> (64 - r));
  }

#define    ROTL32(x, y)    rotl32(x,y)
#define ROTL64(x, y)    rotl64(x,y)

#define BIG_CONSTANT(x) (x##LLU)

#endif // !defined(_MSC_VER)

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

  FORCE_INLINE uint32_t getblock32(const uint32_t *p, int i) {
    return p[i];
  }

  FORCE_INLINE uint64_t getblock64(const uint64_t *p, int i) {
    return p[i];
  }

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

  FORCE_INLINE uint32_t fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
  }

//----------

  FORCE_INLINE uint64_t fmix64(uint64_t k) {
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xff51afd7ed558ccd);
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
    k ^= k >> 33;

    return k;
  }

  inline uint32_t MurmurHash3_x86_32(void *key, int length,
                                     uint32_t seed) {
    const uint8_t *data = (const uint8_t *) key;
    const int nblocks = length / 4;

    uint32_t h1 = seed;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    //----------
    // body

    const uint32_t *blocks = (const uint32_t *) (data + nblocks * 4);

    for (int i = -nblocks; i; i++) {
      uint32_t k1 = getblock32(blocks, i);

      k1 *= c1;
      k1 = ROTL32(k1, 15);
      k1 *= c2;

      h1 ^= k1;
      h1 = ROTL32(h1, 13);
      h1 = h1 * 5 + 0xe6546b64;
    }

    //----------
    // tail

    const uint8_t *tail = (const uint8_t *) (data + nblocks * 4);

    uint32_t k1 = 0;

    switch (length & 3) {
    case 3: k1 ^= tail[2] << 16;
    case 2: k1 ^= tail[1] << 8;
    case 1: k1 ^= tail[0];
      k1 *= c1;
      k1 = ROTL32(k1, 15);
      k1 *= c2;
      h1 ^= k1;
    };

    //----------
    // finalization

    h1 ^= length;

    h1 = fmix32(h1);

    //(uint32_t*)out = h1;
    return h1;
  }

  std::size_t operator()(T t) {
    return MurmurHash3_x86_32(&t, len, 0);
  }
};

struct UInt128Hash {
  std::size_t rotate_by_at_least_1(std::size_t __val, int __shift) const {
    return (__val >> __shift) | (__val << (64 - __shift));
  }

  std::size_t hash_len_16(std::size_t __u, std::size_t __v) const {
    const std::size_t __mul = 0x9ddfea08eb382d69ULL;
    std::size_t __a = (__u ^ __v) * __mul;
    __a ^= (__a >> 47);
    std::size_t __b = (__v ^ __a) * __mul;
    __b ^= (__b >> 47);
    __b *= __mul;
    return __b;
  }
  UInt128Hash() = default;
  inline std::size_t operator()(__uint128_t data) const {
    const __uint128_t __mask = static_cast<std::size_t>(-1);
    const std::size_t __a = (std::size_t) (data & __mask);
    const std::size_t __b = (std::size_t) ((data & (__mask << 64)) >> 64);
    return hash_len_16(__a, rotate_by_at_least_1(__b + 16, 16)) ^ __b;
  }
};