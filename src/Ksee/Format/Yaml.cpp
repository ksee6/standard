#include "../../../include/Ksee/Format/Yaml.hpp"
#include "../../../include/Ksee/Map.hpp"
#include "../../../include/Ksee/Types.hpp"
#include <stdio.h>

namespace Ksee {



static inline bool isSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static inline void emitIdent(String &str, int count) {
  for (int i = 0; i < count; i++)
    str += " ";
}

struct YamlParser {
  const String &input;
  usz i = 0;
  int currentIndent = 0;
  bool isNewLine = true;
  Map<String, Tree<void> *> anchors;

  YamlParser(const String &s) : input(s) {}

  void skipSpace() {
    while (i < input.length()) {
      char c = input.charAt(i);
      if (c == ' ') {
        if (isNewLine)
          currentIndent++;
        i++;
      } else if (c == '\t') {
        if (isNewLine)
          currentIndent += 4;
        i++;
      } else if (c == '\r' || c == '\n') {
        isNewLine = true;
        currentIndent = 0;
        i++;
      } else {
        isNewLine = false;
        break;
      }
    }
  }

  void skipComments(Tree<void> *node = nullptr) {
    while (i < input.length()) {
      skipSpace();
      if (i + 1 < input.length()) {
        char c1 = input.charAt(i);
        char c2 = input.charAt(i + 1);
        if (c1 == '#') {
          i++;
          String comm;
          while (i < input.length() && input.charAt(i) != '\n') {
            comm += input.charAt(i++);
          }
          if (node) {
            auto *c = new Tree<String>(comm.trim());
            c->path = "_comment";
            node->children.push(c);
          }
          isNewLine = true;
          currentIndent = 0;
        } else if (c1 == '/' && c2 == '/' && (i == 0 || input.charAt(i - 1) == ' ' || input.charAt(i - 1) == '\t' || input.charAt(i - 1) == '\n')) {
          i += 2;
          String comm;
          while (i < input.length() && input.charAt(i) != '\n') {
            comm += input.charAt(i++);
          }
          if (node) {
            auto *c = new Tree<String>(comm.trim());
            c->path = "_comment";
            node->children.push(c);
          }
          isNewLine = true;
          currentIndent = 0;
        } else if (c1 == '/' && c2 == '*' && (i == 0 || input.charAt(i - 1) == ' ' || input.charAt(i - 1) == '\t' || input.charAt(i - 1) == '\n')) {
          i += 2;
          String comm;
          while (i + 1 < input.length() &&
                 !(input.charAt(i) == '*' && input.charAt(i + 1) == '/')) {
            comm += input.charAt(i++);
          }
          i += 2;
          if (node) {
            auto *c = new Tree<String>(comm.trim());
            c->path = "_comment";
            node->children.push(c);
          }
        } else {
          break;
        }
      } else if (i < input.length() && input.charAt(i) == '#') {
        i++;
        break;
      } else {
        break;
      }
    }
  }

  String parseString() {
    String res;
    char quote = input.charAt(i);
    bool isQuoted = (quote == '\"' || quote == '\x27');
    if (isQuoted) {
      i++;
      while (i < input.length()) {
        char c = input.charAt(i);
        if (c == quote) {
          i++;
          break;
        }
        if (c == '\\' && i + 1 < input.length()) {
          i++;
          char esc = input.charAt(i);
          if (esc == 'n')
            res += '\n';
          else if (esc == 'r')
            res += '\r';
          else if (esc == 't')
            res += '\t';
          else
            res += esc;
        } else {
          res += c;
        }
        i++;
      }
    } else {
      while (i < input.length()) {
        char c = input.charAt(i);
        if (c == ':' || c == ',' || c == ']' || c == '}' || c == '#' ||
            c == '\n' || c == '\r')
          break;
        res += c;
        i++;
      }
      res = res.trim();
    }
    return res;
  }

  Tree<void> *parseValue(int parentIndent, Tree<void> *parentBranch = nullptr) {
    while (true) {
      skipComments(parentBranch);
      if (i >= input.length())
        return nullptr;

      // Skip Directives (%YAML, %TAG) at root level
      if (currentIndent == 0 && input.charAt(i) == '%' &&
          (input.indexOf("%YAML", i) == i || input.indexOf("%TAG", i) == i)) {
        while (i < input.length() && input.charAt(i) != '\n')
          i++;
        isNewLine = true;
        currentIndent = 0;
        continue;
      }

      // Skip Document Separators (---)
      if (input.charAt(i) == '-' && i + 2 < input.length() &&
          input.charAt(i + 1) == '-' && input.charAt(i + 2) == '-') {
        i += 3;
        while (i < input.length() && input.charAt(i) != '\n')
          i++;
        isNewLine = true;
        currentIndent = 0;
        continue;
      }
      break;
    }

    if (i >= input.length())
      return nullptr;

    // Check for Anchors & Aliases
    if (input.charAt(i) == '&') {
      i++;
      String anchorName;
      while (i < input.length() && !isSpace(input.charAt(i)) &&
             input.charAt(i) != '\n') {
        anchorName += input.charAt(i++);
      }
      Tree<void> *val = parseValue(parentIndent, parentBranch);
      if (val)
        anchors.put(anchorName, val);
      return val;
    }
    if (input.charAt(i) == '*') {
      i++;
      String aliasName;
      while (i < input.length() && !isSpace(input.charAt(i)) &&
             input.charAt(i) != '\n') {
        aliasName += input.charAt(i++);
      }
      Tree<void> **ref = anchors.get(aliasName);
      if (ref && *ref)
        return (*ref)->clone();
      return nullptr;
    }

    int blockIndent = currentIndent;

    // Flow Style Sequence
    if (input.charAt(i) == '[') {
      i++;
      auto *arr = new Tree<void>();
      while (i < input.length()) {
        skipComments(arr);
        if (input.charAt(i) == ']') {
          i++;
          break;
        }
        Tree<void> *val = parseValue(blockIndent, arr);
        if (val)
          arr->children.push(val);
        skipSpace();
        if (input.charAt(i) == ',') {
          i++;
        }
      }
      return arr;
    }

    // Flow Style Mapping
    if (input.charAt(i) == '{') {
      i++;
      auto *mapNode = new Tree<void>();
      while (i < input.length()) {
        skipComments(mapNode);
        if (input.charAt(i) == '}') {
          i++;
          break;
        }
        String k = parseString();
        skipSpace();
        if (input.charAt(i) == ':') {
          i++;
        }
        Tree<void> *val = parseValue(blockIndent, mapNode);
        if (val) {
          val->path = k;
          mapNode->children.push(val);
        }
        skipSpace();
        if (input.charAt(i) == ',') {
          i++;
        }
      }
      return mapNode;
    }

    // Block Sequence
    if (input.charAt(i) == '-' && (i + 1 < input.length() && isSpace(input.charAt(i + 1)))) {
      auto *arr = new Tree<void>();
      int seqIndent = currentIndent;
      while (i < input.length()) {
        skipComments(arr);
        if (currentIndent < seqIndent)
          break;
        if (input.charAt(i) != '-')
          break;
        i++; // skip '-'
        skipSpace();
        Tree<void> *val = parseValue(seqIndent + 1, arr);
        if (val)
          arr->children.push(val);
        skipComments(arr);
      }
      return arr;
    }

    // Look ahead to check if this is a key-value mapping
    bool isKey = false;
    usz peek = i;
    if (peek < input.length() && (input.charAt(peek) == '\"' || input.charAt(peek) == '\x27')) {
      char q = input.charAt(peek++);
      while (peek < input.length()) {
        if (input.charAt(peek) == '\\' && peek + 1 < input.length()) {
          peek += 2;
          continue;
        }
        if (input.charAt(peek) == q) {
          peek++;
          break;
        }
        peek++;
      }
      while (peek < input.length() && isSpace(input.charAt(peek))) peek++;
      if (peek < input.length() && input.charAt(peek) == ':' &&
          (peek + 1 >= input.length() || isSpace(input.charAt(peek + 1)) || input.charAt(peek + 1) == '\n' || input.charAt(peek + 1) == '\r')) {
        isKey = true;
      }
    } else {
      while (peek < input.length()) {
        char cp = input.charAt(peek);
        if (cp == '\n' || cp == '\r')
          break;
        if (cp == ':' && (peek + 1 >= input.length() || isSpace(input.charAt(peek + 1)) || input.charAt(peek + 1) == '\n' || input.charAt(peek + 1) == '\r')) {
          isKey = true;
          break;
        }
        peek++;
      }
    }

    if (isKey) {
      auto *branch = new Tree<void>();
      while (i < input.length()) {
        skipComments(branch);
        if (currentIndent < blockIndent)
          break;
        if (i >= input.length())
          break;

        if (input.charAt(i) == '-' && (i + 1 >= input.length() || isSpace(input.charAt(i + 1))))
          break;

        String k;
        if (input.charAt(i) == '\"' || input.charAt(i) == '\x27') {
          k = parseString();
          skipSpace();
          if (i < input.length() && input.charAt(i) == ':') i++;
        } else {
          while (i < input.length()) {
            char ck = input.charAt(i);
            if (ck == '\n' || ck == '\r') break;
            if (ck == ':' && (i + 1 >= input.length() || isSpace(input.charAt(i + 1)) || input.charAt(i + 1) == '\n' || input.charAt(i + 1) == '\r')) {
              i++;
              break;
            }
            k += ck;
            i++;
          }
        }
        k = k.trim();
        if (k.isEmpty()) break;

        Tree<void> *v = parseValue(blockIndent, branch);
        if (v) {
          v->path = k;
          branch->children.push(v);
        }

        skipComments(branch);
        if (currentIndent < blockIndent) break;
      }
      return branch;
    }

    // Scalar
    String s = parseString();
    if (s == "true")
      return new Tree<bool>(true);
    if (s == "false")
      return new Tree<bool>(false);
    if (s == "null" || s == "~") {
      auto *n = new Tree<void>();
      // null node
      return n;
    }

    bool isNum = true;
    int dotCount = 0;
    for (usz k = 0; k < s.length(); ++k) {
      if (s.charAt(k) == '.')
        dotCount++;
      else if (s.charAt(k) < '0' || s.charAt(k) > '9') {
        if (k != 0 || s.charAt(k) != '-')
          isNum = false;
      }
    }
    if (isNum && s.length() > 0 && s != "-" && dotCount <= 1) {
      if (dotCount == 1)
        return new Tree<f64>(s.toDouble());
      return new Tree<long long>(s.toInt());
    }
    return new Tree<String>(s);
  }
};

bool Yaml::parse(const String &yaml, Tree<void> &outRoot) {
  YamlParser p(yaml);
  Tree<void> *root = &outRoot;
  if (!root)
    return false;

  while (p.i < yaml.length()) {
    Tree<void> *res = p.parseValue(-1, root);
    if (!res)
      break;

    if (res) {
      if (!res->hasValue()) {
        for (usz i = 0; i < res->children.size(); ++i) {
          if (res->children[i])
            root->children.push(res->children[i]->clone());
        }
        delete res;
      } else {
        root->children.push(res);
      }
    }
    p.skipSpace();
  }
  return true;
}

static bool isNodeArray(const Tree<void> *node) {
  if (!node || node->children.isEmpty()) return false;
  for (usz i = 0; i < node->children.size(); ++i) {
    if (!node->children[i]->path.isEmpty()) return false;
  }
  return true;
}

static String emitValue(const Tree<void> *node, int indentLevel, int indentSize, bool firstLineNoIndent = false) {
  if (!node || node->isNull())
    return "null";

  if (node->hasValue()) {
    if (auto s = dynamic_cast<const Tree<String> *>(node))
      return s->value;
    if (auto in = dynamic_cast<const Tree<int> *>(node))
      return String(in->value);
    if (auto i = dynamic_cast<const Tree<long long> *>(node))
      return String(i->value);
    if (auto u = dynamic_cast<const Tree<u64> *>(node))
      return String(u->value);
    if (auto uz = dynamic_cast<const Tree<usz> *>(node))
      return String(uz->value);
    if (auto b = dynamic_cast<const Tree<bool> *>(node))
      return b->value ? "true" : "false";
    if (auto f = dynamic_cast<const Tree<f64> *>(node))
      return String(f->value);
    if (auto f3 = dynamic_cast<const Tree<f32> *>(node))
      return String(f3->value);
    return "null";
  }

  // Array (children all without path)
  if (isNodeArray(node)) {
    if (node->size() == 0)
      return "[]";
    String res;
    bool first = true;
    for (usz i = 0; i < node->children.size(); ++i) {
      Tree<void> *child = node->children[i];
      if (!child) continue;
      if (child->path.toString() == "_comment") {
        if (!(first && firstLineNoIndent)) {
          emitIdent(res, (indentLevel) * indentSize);
        }
        if (auto comm = dynamic_cast<const Tree<String> *>(child))
          res += "# " + comm->value + "\n";
        continue;
      }
      if (!(first && firstLineNoIndent)) {
        emitIdent(res, (indentLevel) * indentSize);
      }
      first = false;
      res += "- " + emitValue(child, indentLevel + 1, indentSize, true) + "\n";
    }
    return res;
  }

  // Object (children with paths)
  if (node->size() == 0)
    return "{}";
  String res;
  for (usz i = 0; i < node->children.size(); ++i) {
    Tree<void> *child = node->children[i];
    if (!child) continue;
    if (child->path.toString() == "_comment") {
      if (!(i == 0 && firstLineNoIndent)) {
        emitIdent(res, (indentLevel) * indentSize);
      }
      if (auto comm = dynamic_cast<const Tree<String> *>(child))
        res += "# " + comm->value + "\n";
      continue;
    }
    if (!(i == 0 && firstLineNoIndent)) {
      emitIdent(res, (indentLevel) * indentSize);
    }
    res += child->path.toString() + ":";
    if (!child->hasValue() && child->size() > 0) {
      res += "\n";
      res += emitValue(child, indentLevel + 1, indentSize);
    } else {
      res += " " + emitValue(child, indentLevel, indentSize, true) + "\n";
    }
  }
  return res;
}

static String escapeJSON(const String &s) {
  String out = "\"";
  for (usz i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c == '\"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\b') out += "\\b";
    else if (c == '\f') out += "\\f";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if (static_cast<unsigned char>(c) < 0x20) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
      out += buf;
    } else {
      out.push(static_cast<u8>(c));
    }
  }
  out += "\"";
  return out;
}

static String emitJSON(const Tree<void> *node, int indentLevel, int indentSize) {
  if (!node || node->isNull()) return "null";

  if (node->hasValue()) {
    if (auto s = dynamic_cast<const Tree<String> *>(node))
      return escapeJSON(s->value);
    if (auto i = dynamic_cast<const Tree<long long> *>(node))
      return String(i->value);
    if (auto in = dynamic_cast<const Tree<int> *>(node))
      return String(in->value);
    if (auto u = dynamic_cast<const Tree<u64> *>(node))
      return String(u->value);
    if (auto uz = dynamic_cast<const Tree<usz> *>(node))
      return String(uz->value);
    if (auto b = dynamic_cast<const Tree<bool> *>(node))
      return b->value ? "true" : "false";
    if (auto f = dynamic_cast<const Tree<f64> *>(node))
      return String(f->value);
    if (auto f3 = dynamic_cast<const Tree<f32> *>(node))
      return String(f3->value);
    return "null";
  }

  if (isNodeArray(node)) {
    if (node->size() == 0) return "[]";
    String res = "[\n";
    bool first = true;
    for (usz i = 0; i < node->children.size(); ++i) {
      Tree<void> *child = node->children[i];
      if (!child || child->path.toString() == "_comment") continue;
      if (!first) res += ",\n";
      first = false;
      emitIdent(res, (indentLevel + 1) * indentSize);
      res += emitJSON(child, indentLevel + 1, indentSize);
    }
    res += "\n";
    emitIdent(res, indentLevel * indentSize);
    res += "]";
    return res;
  }

  // Object
  if (node->size() == 0) return "{}";
  String res = "{\n";
  bool first = true;
  for (usz i = 0; i < node->children.size(); ++i) {
    Tree<void> *child = node->children[i];
    if (!child || child->path.toString() == "_comment") continue;
    if (!first) res += ",\n";
    first = false;
    emitIdent(res, (indentLevel + 1) * indentSize);
    res += escapeJSON(child->path.toString()) + ": " + emitJSON(child, indentLevel + 1, indentSize);
  }
  res += "\n";
  emitIdent(res, indentLevel * indentSize);
  res += "}";
  return res;
}

String Yaml::toYAML(const Tree<void> &root, int indent) {
  return emitValue(&root, 0, indent);
}

String Yaml::toJSON(const Tree<void> &root, int indent) {
  return emitJSON(&root, 0, indent);
}

} // namespace Ksee
