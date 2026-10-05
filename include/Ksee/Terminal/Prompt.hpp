/**
 * @file Prompt.hpp
 * @brief Interactive terminal prompts for user input in Ksee.
 */

#ifndef KSEE_TERMINAL_PROMPT_HPP
#define KSEE_TERMINAL_PROMPT_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "../Array.hpp"

namespace Ksee {

String ask(const String &question, const String &defaultVal = "");
String password(const String &question);
bool confirm(const String &question, bool defaultVal = true);
usz select(const String &question, const Array<String> &options);
Array<usz> multiSelect(const String &question, const Array<String> &options);
String readLine(const String &prompt, Array<String> *history = nullptr);

} // namespace Ksee

#endif // KSEE_TERMINAL_PROMPT_HPP

