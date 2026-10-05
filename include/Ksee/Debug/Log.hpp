/**
 * @file Log.hpp
 * @brief Thread-safe logging utility for Ksee.
 */

#ifndef KSEE_DEBUG_LOG_HPP
#define KSEE_DEBUG_LOG_HPP

#include "../Types.hpp"
#include "../String.hpp"

#if !defined(__KERNEL__)
#include <unistd.h>
#endif

namespace Ksee {

enum class LogLevel : u8 {
  None = 0,
  Verbose = 1,
  Info = 2,
  Warning = 3,
  Error = 4,
  Critical = 5
};

class Log {
private:
  alignas(4) unsigned int lock_val = 0;
  LogLevel currentLevel = LogLevel::Info;

  Log() {}
  Log(const Log &) = delete;
  Log &operator=(const Log &) = delete;

  void lock();
  void unlock();
  void print_unlocked(const String &msg);
  void println_unlocked();

public:
  static Log &getInstance();

  void setLevel(LogLevel l) { currentLevel = l; }

  void print(const String &msg);
  void println(const String &msg);
  void println();

  void verbose(const String &msg);
  void info(const String &msg);
  void warn(const String &msg);
  void error(const String &msg);
  void critical(const String &msg);
};

inline void print(const String &msg) { Log::getInstance().print(msg); }
inline void println(const String &msg) { Log::getInstance().println(msg); }
inline void println() { Log::getInstance().println(); }
inline void info(const String &msg) { Log::getInstance().info(msg); }
inline void warn(const String &msg) { Log::getInstance().warn(msg); }
inline void error(const String &msg) { Log::getInstance().error(msg); }

} // namespace Ksee

#endif // KSEE_DEBUG_LOG_HPP

