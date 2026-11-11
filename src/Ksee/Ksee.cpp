/**
 * @file Ksee.cpp
 * @brief Implementation of basic random, uptime clock, and hash mix for Ksee.
 */

#include "../../include/Ksee/Ksee.hpp"
#include "../../include/Ksee/String.hpp"
#include "../../include/Ksee/Array.hpp"
#include <string.h>
#include <time.h>

#if !defined(__KERNEL__) && !defined(KSEE_NO_STD)
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#if __has_include(<sys/mman.h>) && defined(__linux__)
#include <sys/mman.h>
#endif
#endif

#if defined(_WIN32)
extern "C" __declspec(dllimport) unsigned char __stdcall SystemFunction036(void* RandomBuffer, unsigned long RandomBufferLength);
#endif

namespace Ksee {

usz fnvHashMix(usz k) {
#if __SIZEOF_POINTER__ == 8
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccdULL;
  k ^= k >> 33;
  k *= 0xc4ceb9fe1a85ec53ULL;
  k ^= k >> 33;
#else
  k ^= k >> 16;
  k *= 0x85ebca6b;
  k ^= k >> 13;
  k *= 0xc2b2ae35;
  k ^= k >> 16;
#endif
  return k;
}

// -------------------------------------------------------------------------
// Basic Random Implementation
// -------------------------------------------------------------------------

alignas(64) static u32 _randomPool[20] = {
    0x243f6a88, 0x85a308d3, 0x13198a2e, 0x03707344,
    0xa4093822, 0x299f31d0, 0x082efa98, 0xec4e6c89,
    0x452821e6, 0x38d01377, 0xbe5466cf, 0x34e90c6c,
    0xc0ac29b7, 0xc97c50dd, 0x3f84d5b5, 0xb5470917,
    0x9216d5d9, 0x8979fb1b, 0xd1310ba6, 0x98dfb5ac
};
static bool _randomInitialized = false;
static u32 _randomCounter = 0;

static inline u32 rotl32(u32 v, int n) {
  return (v << n) | (v >> (32 - n));
}

static inline void quarterRound(u32 &a, u32 &b, u32 &c, u32 &d) {
  a += b; d ^= a; d = rotl32(d, 16);
  c += d; b ^= c; b = rotl32(b, 12);
  a += b; d ^= a; d = rotl32(d, 8);
  c += d; b ^= c; b = rotl32(b, 7);
}

static void advancePool() {
  _randomCounter++;
  _randomPool[16] += _randomCounter;
  _randomPool[17] ^= _randomPool[16];
  _randomPool[18] += _randomPool[17];
  _randomPool[19] ^= rotl32(_randomPool[18], 13);

  quarterRound(_randomPool[0], _randomPool[4], _randomPool[8],  _randomPool[12]);
  quarterRound(_randomPool[1], _randomPool[5], _randomPool[9],  _randomPool[13]);
  quarterRound(_randomPool[2], _randomPool[6], _randomPool[10], _randomPool[14]);
  quarterRound(_randomPool[3], _randomPool[7], _randomPool[11], _randomPool[15]);

  quarterRound(_randomPool[0], _randomPool[5], _randomPool[10], _randomPool[15]);
  quarterRound(_randomPool[1], _randomPool[6], _randomPool[11], _randomPool[12]);
  quarterRound(_randomPool[2], _randomPool[7], _randomPool[8],  _randomPool[13]);
  quarterRound(_randomPool[3], _randomPool[4], _randomPool[9],  _randomPool[14]);

  _randomPool[0] ^= _randomPool[16];
  _randomPool[5] ^= _randomPool[17];
  _randomPool[10] ^= _randomPool[18];
  _randomPool[15] ^= _randomPool[19];
}

void randomSeed(u64 seed, bool overwrite) {
  if (!overwrite && _randomInitialized) return;
  u64 s = seed;
  for (int i = 0; i < 20; i++) {
    s += 0x9e3779b97f4a7c15ULL;
    u64 z = s;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    _randomPool[i] = (u32)(z ^ (z >> 31));
  }
  _randomCounter = 0;
  for (int i = 0; i < 4; i++) advancePool();
  _randomInitialized = true;
}

void randomSeed(bool overwrite) {
  if (!overwrite && _randomInitialized) return;
  bool filled = false;
#if defined(__linux__) || defined(__APPLE__)
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd >= 0) {
    ssize_t n = read(fd, _randomPool, sizeof(_randomPool));
    close(fd);
    if (n == (ssize_t)sizeof(_randomPool)) filled = true;
  }
#elif defined(_WIN32)
  if (SystemFunction036(_randomPool, sizeof(_randomPool))) filled = true;
#endif

  if (filled) {
    _randomInitialized = true;
    _randomCounter = 0;
    advancePool();
  } else {
    u64 entropy = 0x9e3779b97f4a7c15ULL;
#if defined(MECA_EMBEDDED)
    struct timespec ts = {0, 0};
    clock_gettime(0, &ts);
    entropy ^= ((u64)ts.tv_sec << 32) | (u64)ts.tv_nsec;
    entropy ^= (u64)(usz)&_randomPool;
#elif defined(__XTENSA__)
    unsigned ccount = 0;
    __asm__ volatile("rsr %0, ccount" : "=a"(ccount));
    entropy ^= (u64)ccount;
    entropy ^= (u64)(usz)&_randomPool;
#elif !defined(_WIN32)
    struct timeval tv;
    if (gettimeofday(&tv, nullptr) == 0) {
      entropy ^= ((u64)tv.tv_sec << 32) | (u64)tv.tv_usec;
    }
    entropy ^= (u64)(usz)&_randomPool;
#else
    entropy ^= (u64)(usz)&_randomPool;
#endif
    randomSeed(entropy, overwrite);
  }
}

u32 random() {
  if (!_randomInitialized) randomSeed(false);
  advancePool();
  return _randomPool[0] ^ _randomPool[12];
}

u32 random(u32 max) {
  if (max == 0) return 0;
  return random() % max;
}

i32 random(i32 min, i32 max) {
  if (min >= max) return min;
  return min + (i32)(random() % (u32)(max - min));
}

void randomFill(u8 *buffer, usz size) {
  if (!buffer || size == 0) return;
  if (!_randomInitialized) randomSeed(false);

  usz offset = 0;
  while (offset + 64 <= size) {
    advancePool();
    memcpy(buffer + offset, _randomPool, 64);
    offset += 64;
  }
  if (offset < size) {
    advancePool();
    memcpy(buffer + offset, _randomPool, size - offset);
  }
}

// -------------------------------------------------------------------------
// Uptime Clock Implementation
// -------------------------------------------------------------------------

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__) || defined(MECA_EMBEDDED) || defined(__EMSCRIPTEN__) || defined(__wasm__) || defined(ESP_PLATFORM) || defined(ARDUINO)
static u64 getBootNanos() {
  struct timespec ts = {0, 0};
#if defined(CLOCK_BOOTTIME)
  clock_gettime(CLOCK_BOOTTIME, &ts);
#elif defined(CLOCK_MONOTONIC)
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  clock_gettime(1, &ts);
#endif
  return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
}

u64 nanos() {
  return getBootNanos();
}

u64 millis() {
  return getBootNanos() / 1000000ULL;
}

u64 micros() {
  return getBootNanos() / 1000ULL;
}
#elif defined(_WIN32)
#include <windows.h>
u64 nanos() {
  static LARGE_INTEGER freq;
  static BOOL init = QueryPerformanceFrequency(&freq);
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return (u64)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
}
u64 millis() { return nanos() / 1000000ULL; }
u64 micros() { return nanos() / 1000ULL; }
#elif defined(__XTENSA__)
// Bare-metal Xtensa: read CCOUNT register (cycles), assume 240 MHz clock.
u64 nanos() {
    unsigned ccount = 0;
    __asm__ volatile("rsr %0, ccount" : "=a"(ccount));
    // 240 MHz → 1 tick ≈ 4.167 ns; multiply by 1000/240 ≈ 25/6
    return (u64)ccount * 25ULL / 6ULL;
}
u64 millis() { return nanos() / 1000000ULL; }
u64 micros() { return nanos() / 1000ULL; }
#else
// Generic bare-metal fallback (no POSIX time()).
u64 nanos()  { return 0; }
u64 millis() { return 0; }
u64 micros() { return 0; }
#endif

// -------------------------------------------------------------------------
// Epoch Time Auto-Set Implementation
// -------------------------------------------------------------------------

bool isEpochTimeSet = false;
u64 epochOnStart = 0;
i32 gmt = 0;

bool autoSetEpochTime() {
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__) || defined(__EMSCRIPTEN__) || defined(__wasm__) || defined(ESP_PLATFORM) || defined(ARDUINO)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0 && ts.tv_sec > 0) {
        u64 nowMs = (u64)ts.tv_sec * 1000ULL + (u64)(ts.tv_nsec / 1000000ULL);
        u64 upMs = millis();
        epochOnStart = (nowMs >= upMs) ? (nowMs - upMs) : 0;
        isEpochTimeSet = true;
        time_t sec = ts.tv_sec;
        struct tm tm_buf;
        if (localtime_r(&sec, &tm_buf)) {
#if defined(__USE_MISC) || defined(_BSD_SOURCE) || defined(__APPLE__) || defined(__linux__)
            gmt = (i32)tm_buf.tm_gmtoff;
#endif
        }
        return true;
    }
    return false;
#elif defined(_WIN32)
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    u64 nowMs = (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
    u64 upMs = millis();
    epochOnStart = (nowMs >= upMs) ? (nowMs - upMs) : 0;
    isEpochTimeSet = true;
    TIME_ZONE_INFORMATION tzInfo;
    DWORD tzRes = GetTimeZoneInformation(&tzInfo);
    long bias = tzInfo.Bias;
    if (tzRes == TIME_ZONE_ID_DAYLIGHT) bias += tzInfo.DaylightBias;
    else if (tzRes == TIME_ZONE_ID_STANDARD) bias += tzInfo.StandardBias;
    gmt = (i32)(-bias * 60);
    return true;
#elif defined(ESP_PLATFORM) || defined(ARDUINO) || defined(__XTENSA__)
    struct timeval tv;
    if (gettimeofday(&tv, nullptr) == 0 && tv.tv_sec > 0) {
        u64 nowMs = (u64)tv.tv_sec * 1000ULL + (u64)(tv.tv_usec / 1000ULL);
        u64 upMs = millis();
        epochOnStart = (nowMs >= upMs) ? (nowMs - upMs) : 0;
        isEpochTimeSet = true;
        time_t sec = tv.tv_sec;
        struct tm tm_buf;
        if (localtime_r(&sec, &tm_buf)) {
#if defined(__USE_MISC) || defined(_BSD_SOURCE) || defined(__APPLE__) || defined(__linux__)
            gmt = (i32)tm_buf.tm_gmtoff;
#endif
        }
        return true;
    }
    return false;
#else
    return false;
#endif
}

#if defined(__EMSCRIPTEN__)
extern "C" double emscripten_date_now(void);
u64 epochMillis() {
    if (!isEpochTimeSet) {
        autoSetEpochTime();
    }
    return (u64)emscripten_date_now();
}
#else
u64 epochMillis() {
    if (!isEpochTimeSet) {
        autoSetEpochTime();
    }
    return epochOnStart + millis();
}
#endif

u64 epochTime() {
    return epochMillis();
}

i64 systemStartMicros = 0;

void syncClock(i64 now) {
    systemStartMicros = now - (i64)micros();
}

void syncClock() {
#if defined(__KERNEL__)
    syncClock(ktime_get_real_ns() / 1000ULL);
#elif defined(_WIN32)
    unsigned long long ft;
    ::GetSystemTimePreciseAsFileTime((FILETIME*)&ft);
    syncClock((i64)((ft - 116444736000000000ULL) / 10ULL));
#elif defined(KSEE_NO_STD) && defined(__linux__)
  #if defined(__x86_64__)
    struct {
      long long tv_sec;
      long long tv_nsec;
    } ts;
    long ret;
    __asm__ volatile (
      "syscall"
      : "=a"(ret)
      : "a"(228), "D"(0), "S"(&ts)
      : "rcx", "r11", "memory"
    );
    if (ret == 0) {
      syncClock((i64)ts.tv_sec * 1000000ULL + (i64)(ts.tv_nsec / 1000));
    }
  #endif
#elif defined(ARDUINO)
    syncClock(0);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    syncClock((i64)ts.tv_sec * 1000000ULL + (i64)(ts.tv_nsec / 1000));
#endif
}

u128::u128(const String &str) : data{} {
    usz take = str.size() < 16 ? str.size() : 16;
    for (usz i = 0; i < take; ++i) data[i] = (u8)str[i];
}

String u128::asString() const {
    String res;
    res.pushEach(data, 16);
    return res;
}

String u128::toHex() const {
    static const char hexChars[] = "0123456789abcdef";
    String res;
    for (usz i = 0; i < 16; ++i) {
        res.push((u8)hexChars[(data[i] >> 4) & 0x0F]);
        res.push((u8)hexChars[data[i] & 0x0F]);
    }
    return res;
}

u256::u256(const String &str) : data{} {
    usz take = str.size() < 32 ? str.size() : 32;
    for (usz i = 0; i < take; ++i) data[i] = (u8)str[i];
}

String u256::asString() const {
    String res;
    res.pushEach(data, 32);
    return res;
}

String u256::toHex() const {
    static const char hexChars[] = "0123456789abcdef";
    String res;
    for (usz i = 0; i < 32; ++i) {
        res.push((u8)hexChars[(data[i] >> 4) & 0x0F]);
        res.push((u8)hexChars[data[i] & 0x0F]);
    }
    return res;
}

u512::u512(const String &str) : data{} {
    usz take = str.size() < 64 ? str.size() : 64;
    for (usz i = 0; i < take; ++i) data[i] = (u8)str[i];
}

String u512::asString() const {
    String res;
    res.pushEach(data, 64);
    return res;
}

String u512::toHex() const {
    static const char hexChars[] = "0123456789abcdef";
    String res;
    for (usz i = 0; i < 64; ++i) {
        res.push((u8)hexChars[(data[i] >> 4) & 0x0F]);
        res.push((u8)hexChars[data[i] & 0x0F]);
    }
    return res;
}

} // namespace Ksee

