/**
 * @file Box.hpp
 * @brief Terminal UI box formatting, tables, and hyperlinks for Ksee.
 */

#ifndef KSEE_TERMINAL_BOX_HPP
#define KSEE_TERMINAL_BOX_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "../Array.hpp"

namespace Ksee {

String Box(const String &content, const String &title = "");
String Table(const Array<String> &arr);
String Link(const String &text, const String &url);

} // namespace Ksee

#endif // KSEE_TERMINAL_BOX_HPP

