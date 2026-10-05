/**
 * @file Color.cpp
 * @brief ANSI styling and 24-bit TrueColor RGB terminal colors implementation.
 */

#include "../../../include/Ksee/Terminal/Color.hpp"

namespace Ksee {

String Bold(const String &s) { return "\x1b[1m" + s + "\x1b[22m"; }
String Italic(const String &s) { return "\x1b[3m" + s + "\x1b[23m"; }
String Underline(const String &s) { return "\x1b[4m" + s + "\x1b[24m"; }
String Strike(const String &s) { return "\x1b[9m" + s + "\x1b[29m"; }
String Dim(const String &s) { return "\x1b[2m" + s + "\x1b[22m"; }

String RGB(const String &s, u8 r, u8 g, u8 b) {
  return "\x1b[38;2;" + String::fromNumber((int)r) + ";" + String::fromNumber((int)g) + ";" + String::fromNumber((int)b) + "m" + s + "\x1b[0m";
}

String RGB(const String &s, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2) {
  String res;
  usz len = s.length();
  if (len == 0) return "";
  for (usz i = 0; i < len; ++i) {
    float t = (len > 1) ? (float)i / (float)(len - 1) : 0.0f;
    u8 r = (u8)(r1 + t * (r2 - r1));
    u8 g = (u8)(g1 + t * (g2 - g1));
    u8 b = (u8)(b1 + t * (b2 - b1));
    String ch;
    ch.push(s[i]);
    res += RGB(ch, r, g, b);
  }
  return res;
}

String BRGB(const String &s, u8 r, u8 g, u8 b) {
  return "\x1b[48;2;" + String::fromNumber((int)r) + ";" + String::fromNumber((int)g) + ";" + String::fromNumber((int)b) + "m" + s + "\x1b[0m";
}

String BRGB(const String &s, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2) {
  String res;
  usz len = s.length();
  if (len == 0) return "";
  for (usz i = 0; i < len; ++i) {
    float t = (len > 1) ? (float)i / (float)(len - 1) : 0.0f;
    u8 r = (u8)(r1 + t * (r2 - r1));
    u8 g = (u8)(g1 + t * (g2 - g1));
    u8 b = (u8)(b1 + t * (b2 - b1));
    String ch;
    ch.push(s[i]);
    res += BRGB(ch, r, g, b);
  }
  return res;
}

String Cyan(const String &s) { return "\x1b[36m" + s + "\x1b[0m"; }
String Magenta(const String &s) { return "\x1b[35m" + s + "\x1b[0m"; }
String Yellow(const String &s) { return "\x1b[33m" + s + "\x1b[0m"; }
String White(const String &s) { return "\x1b[37m" + s + "\x1b[0m"; }
String Black(const String &s) { return "\x1b[30m" + s + "\x1b[0m"; }
String Gray(const String &s) { return "\x1b[90m" + s + "\x1b[0m"; }
String Red(const String &s) { return "\x1b[31m" + s + "\x1b[0m"; }
String Green(const String &s) { return "\x1b[32m" + s + "\x1b[0m"; }
String Blue(const String &s) { return "\x1b[34m" + s + "\x1b[0m"; }

} // namespace Ksee

