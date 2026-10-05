/**
 * @file Box.cpp
 * @brief Terminal UI box formatting, tables, and hyperlinks implementation.
 */

#include "../../../include/Ksee/Terminal/Box.hpp"
#include "../../../include/Ksee/Terminal/Color.hpp"

namespace Ksee {

static usz visualLength(const String &s) {
  usz len = 0;
  bool inEscape = false;
  for (usz i = 0; i < s.length(); ++i) {
    if (s[i] == '\x1b') inEscape = true;
    else if (inEscape && (s[i] == 'm' || s[i] == '\\')) inEscape = false;
    else if (!inEscape) len++;
  }
  return len;
}

String Link(const String &text, const String &url) {
  return "\x1b]8;;" + url + "\x1b\\" + text + "\x1b]8;;\x1b\\";
}

String Table(const Array<String> &arr) {
  usz cols = 1;
  Array<usz> colWidths;
  colWidths.allocate(cols);
  colWidths[0] = 0;

  usz rows = arr.size();
  for (usz r = 0; r < rows; ++r) {
    usz len = visualLength(arr[r]);
    if (len > colWidths[0]) colWidths[0] = len;
  }

  String res;
  for (usz r = 0; r < rows; ++r) {
    String val = arr[r];
    res += val.padEnd(colWidths[0] + 2 + (val.length() - visualLength(val)));
    res += "\n";
  }
  return res;
}

String Box(const String &content, const String &title) {
  Array<String> lines = content.split("\n");
  usz maxLen = visualLength(title);
  for (usz i = 0; i < lines.size(); ++i) {
    usz vln = visualLength(lines[i]);
    if (vln > maxLen) maxLen = vln;
  }

  String res;
  res += "┌─";
  if (!title.isEmpty()) {
    res += " " + Bold(title) + " ";
    usz visTitle = visualLength(title);
    if (maxLen + 1 > visTitle) {
      for (usz i = 0; i < maxLen + 1 - visTitle; ++i) res += "─";
    }
  } else {
    for (usz i = 0; i < maxLen + 3; ++i) res += "─";
  }
  res += "┐\n";

  for (usz i = 0; i < lines.size(); ++i) {
    usz vln = visualLength(lines[i]);
    usz len = lines[i].length();
    usz targetLen = (len >= vln) ? (maxLen + (len - vln)) : maxLen;
    res += "│  " + lines[i].padEnd(targetLen) + "  │\n";
  }

  res += "└";
  for (usz i = 0; i < maxLen + 4; ++i) res += "─";
  res += "┘\n";

  return res;
}

} // namespace Ksee

