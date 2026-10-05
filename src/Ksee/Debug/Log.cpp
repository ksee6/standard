/**
 * @file Log.cpp
 * @brief Thread-safe logging utility implementation for Ksee.
 */

#include "../../../include/Ksee/Debug/Log.hpp"
#include <stdio.h>

namespace Ksee {

void Log::lock() {
  while (__atomic_test_and_set(&lock_val, __ATOMIC_ACQUIRE)) {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__arm__) || defined(__aarch64__)
    __asm__ volatile("yield" ::: "memory");
#endif
  }
}

void Log::unlock() {
  __atomic_clear(&lock_val, __ATOMIC_RELEASE);
}

void Log::print_unlocked(const String &msg) {
  if (!msg.isEmpty()) {
    ::write(1, msg.data(), msg.length());
  }
}

void Log::println_unlocked() {
  ::write(1, "\n", 1);
}

Log &Log::getInstance() {
  alignas(4) static unsigned int init_lock = 0;
  static bool initialized = false;
  static char storage[sizeof(Log)];
  if (!__atomic_load_n(&initialized, __ATOMIC_ACQUIRE)) {
    while (__atomic_test_and_set(&init_lock, __ATOMIC_ACQUIRE)) {
#if defined(__x86_64__) || defined(__i386__)
      __builtin_ia32_pause();
#elif defined(__arm__) || defined(__aarch64__)
      __asm__ volatile("yield" ::: "memory");
#endif
    }
    if (!initialized) {
      new (storage) Log();
      __atomic_store_n(&initialized, true, __ATOMIC_RELEASE);
    }
    __atomic_clear(&init_lock, __ATOMIC_RELEASE);
  }
  return *reinterpret_cast<Log *>(storage);
}

void Log::print(const String &msg) {
  lock();
  print_unlocked(msg);
  unlock();
}

void Log::println(const String &msg) {
  lock();
  print_unlocked(msg);
  println_unlocked();
  unlock();
}

void Log::println() {
  lock();
  println_unlocked();
  unlock();
}

void Log::verbose(const String &msg) {
  if (currentLevel <= LogLevel::Verbose) {
    println("[VERBOSE] " + msg);
  }
}

void Log::info(const String &msg) {
  if (currentLevel <= LogLevel::Info) {
    println("[INFO] " + msg);
  }
}

void Log::warn(const String &msg) {
  if (currentLevel <= LogLevel::Warning) {
    println("[WARN] " + msg);
  }
}

void Log::error(const String &msg) {
  if (currentLevel <= LogLevel::Error) {
    println("[ERROR] " + msg);
  }
}

void Log::critical(const String &msg) {
  if (currentLevel <= LogLevel::Critical) {
    println("[CRITICAL] " + msg);
  }
}

} // namespace Ksee

