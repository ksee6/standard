/**
 * @file Format.hpp
 * @brief Terminal formatting, colors, and status logging for Ksee.
 */

#ifndef KSEE_TERMINAL_FORMAT_HPP
#define KSEE_TERMINAL_FORMAT_HPP

#include "Color.hpp"
#include "Box.hpp"
#include "Progress.hpp"
#include "Prompt.hpp"
#include "Command.hpp"
#include <cstdio>
#include <cstdlib>

namespace Ksee {
namespace Terminal {

inline void Success(const String &msg) { printf("%s  %s\n", Green("✓").c_str(), msg.c_str()); }
inline void Info(const String &msg)    { printf("%s  %s\n", Blue("ℹ").c_str(), msg.c_str()); }
inline void Warn(const String &msg)    { printf("%s  %s\n", Yellow("⚠").c_str(), msg.c_str()); }
inline void Error(const String &msg)   { printf("%s  %s\n", Red("✗").c_str(), msg.c_str()); }
inline void Fatal(const String &msg)   { printf("%s  %s\n", Red(Bold("✗")).c_str(), Bold(msg).c_str()); exit(1); }

} // namespace Terminal

using namespace Terminal;

} // namespace Ksee

#endif // KSEE_TERMINAL_FORMAT_HPP
