/**
 * @file Color.hpp
 * @brief ANSI styling and 24-bit TrueColor RGB terminal colors for Ksee.
 */

#ifndef KSEE_TERMINAL_COLOR_HPP
#define KSEE_TERMINAL_COLOR_HPP

#include "../Types.hpp"
#include "../String.hpp"

namespace Ksee {

String Bold(const String &s);
String Italic(const String &s);
String Underline(const String &s);
String Strike(const String &s);
String Dim(const String &s);

String RGB(const String &s, u8 r, u8 g, u8 b);
String RGB(const String &s, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2);

String BRGB(const String &s, u8 r, u8 g, u8 b);
String BRGB(const String &s, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2);

String Cyan(const String &s);
String Magenta(const String &s);
String Yellow(const String &s);
String White(const String &s);
String Black(const String &s);
String Gray(const String &s);
String Red(const String &s);
String Green(const String &s);
String Blue(const String &s);

} // namespace Ksee

#endif // KSEE_TERMINAL_COLOR_HPP

