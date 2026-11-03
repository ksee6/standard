/**
 * @file Tree.cpp
 * @brief Complete implementation of Ksee::Tree with native types, YAML/JSON parsing with comments & go-tos,
 *        and ultra-bitpacked petrified tree serialization.
 */

#include "../../include/Ksee/Tree.hpp"
#include "../../include/Ksee/Map.hpp"
#include <stdio.h>
#include <string.h>

namespace Ksee {

// -----------------------------------------------------------------------------
// Construction, Destruction, Lifecycle
// -----------------------------------------------------------------------------

Tree<void>::Tree() {}

Tree<void>::Tree(const Path &p) : path(p) {}

Tree<void>::Tree(const String &p) {
    if (p.size() > 0) {
        if (hydrate(p, *this)) {
            return;
        }
        clear();
    }
    path = Path(p);
}

Tree<void>::Tree(const char *p) : path(Path(p)) {}

Tree<void>::~Tree() {
    clear();
}

void Tree<void>::clear() {
    for (usz i = 0; i < children.size(); ++i) {
        delete children[i];
    }
    children.clear();
    m_forceArray = false;
}

Tree<void>::Tree(const Tree<void> &other) : m_forceArray(other.m_forceArray), path(other.path) {
    for (usz i = 0; i < other.children.size(); ++i) {
        Tree<void>* c = other.children[i]->clone();
        c->parent = this;
        children.push(c);
    }
}

Tree<void>::Tree(Tree<void> &&other) noexcept
    : m_forceArray(other.m_forceArray),
      children(std::move(other.children)),
      parent(other.parent),
      path(std::move(other.path)) {
    other.parent = nullptr;
    other.m_forceArray = false;
    for (usz i = 0; i < children.size(); ++i) {
        children[i]->parent = this;
    }
}

Tree<void>& Tree<void>::operator=(const Tree<void> &other) {
    if (this != &other) {
        clear();
        m_forceArray = other.m_forceArray;
        path = other.path;
        for (usz i = 0; i < other.children.size(); ++i) {
            Tree<void>* c = other.children[i]->clone();
            c->parent = this;
            children.push(c);
        }
    }
    return *this;
}

Tree<void>& Tree<void>::operator=(Tree<void> &&other) noexcept {
    if (this != &other) {
        clear();
        m_forceArray = other.m_forceArray;
        children = std::move(other.children);
        parent = other.parent;
        path = std::move(other.path);
        other.parent = nullptr;
        other.m_forceArray = false;
        for (usz i = 0; i < children.size(); ++i) {
            children[i]->parent = this;
        }
    }
    return *this;
}

Tree<void>* Tree<void>::clone() const {
    Tree<void>* n = new Tree<void>(path);
    n->m_forceArray = m_forceArray;
    for (usz i = 0; i < children.size(); ++i) {
        Tree<void>* c = children[i]->clone();
        c->parent = n;
        n->children.push(c);
    }
    return n;
}

Tree<void>* Tree<void>::addChild(Tree<void>* c) {
    if (c) {
        c->parent = this;
        children.push(c);
    }
    return c;
}

// -----------------------------------------------------------------------------
// Array and Object Identification
// -----------------------------------------------------------------------------

bool Tree<void>::isArray() const {
    if (m_forceArray) return true;
    if (children.isEmpty()) return false;
    for (usz i = 0; i < children.size(); ++i) {
        if (!children[i]->path.isEmpty() && children[i]->path.toString().length() > 0) {
            return false;
        }
    }
    return true;
}

bool Tree<void>::isObject() const {
    if (m_forceArray) return false;
    if (children.isEmpty()) return !hasValue();
    for (usz i = 0; i < children.size(); ++i) {
        if (!children[i]->path.isEmpty() && children[i]->path.toString().length() > 0) {
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Radix Trie Indexing & Query
// -----------------------------------------------------------------------------

static bool isPrefixOf(const Path &prefix, const Path &full) {
    if (prefix.parts.size() > full.parts.size()) return false;
    for (usz i = 0; i < prefix.parts.size(); ++i) {
        if (prefix.parts[i] != full.parts[i]) return false;
    }
    return true;
}

static Path slicePath(const Path &p, usz start) {
    Path out;
    for (usz i = start; i < p.parts.size(); ++i) {
        out.parts.push(p.parts[i]);
    }
    return out;
}

Tree<void>* Tree<void>::get(const Path &target) const {
    if (target.isEmpty()) return const_cast<Tree<void>*>(this);

    for (usz i = 0; i < children.size(); ++i) {
        if (children[i]->path == target) {
            return children[i];
        }
        if (isPrefixOf(children[i]->path, target) && !children[i]->path.isEmpty()) {
            Path remaining = slicePath(target, children[i]->path.parts.size());
            Tree<void>* res = children[i]->get(remaining);
            if (res) return res;
        }
    }
    return nullptr;
}

Tree<void>* Tree<void>::get(const String &target) const {
    return get(Path(target));
}

Tree<void>* Tree<void>::get(const char *target) const {
    return get(Path(target));
}

bool Tree<void>::has(const Path &target) const {
    return get(target) != nullptr;
}

bool Tree<void>::has(const String &target) const {
    return has(Path(target));
}

bool Tree<void>::has(const char *target) const {
    return has(Path(target));
}

bool Tree<void>::deleteKey(const Path &target) {
    if (target.isEmpty()) return false;
    bool deleted = false;
    for (long long i = (long long)children.size() - 1; i >= 0; --i) {
        if (children[(usz)i]->path == target) {
            delete children[(usz)i];
            children.remove((usz)i);
            deleted = true;
        } else if (isPrefixOf(children[(usz)i]->path, target) && !children[(usz)i]->path.isEmpty()) {
            Path remaining = slicePath(target, children[(usz)i]->path.parts.size());
            if (children[(usz)i]->deleteKey(remaining)) {
                if (children[(usz)i]->children.isEmpty() && children[(usz)i]->isNull()) {
                    delete children[(usz)i];
                    children.remove((usz)i);
                }
                deleted = true;
            }
        }
    }
    return deleted;
}

bool Tree<void>::deleteKey(const String &target) {
    return deleteKey(Path(target));
}

bool Tree<void>::deleteKey(const char *target) {
    return deleteKey(Path(target));
}

Tree<void>* Tree<void>::push(const Path &target) {
    Tree<void>* existing = get(target);
    if (existing) return existing;

    Tree<void>* n = new Tree<void>(target);
    n->parent = this;
    children.push(n);
    return n;
}

Tree<void>* Tree<void>::push(Tree<void>* child) {
    return addChild(child);
}

Path Tree<void>::generate(const Path &target) const {
    const Tree<void>* node = (target.isEmpty() || target == this->path) ? this : get(target);
    u64 idx = 0;
    while (true) {
        String suffix = String::fromNumber(idx);
        bool exists = false;
        if (node) {
            for (usz i = 0; i < node->children.size(); ++i) {
                if (node->children[i]) {
                    String cName = node->children[i]->path.toString();
                    if (cName == suffix || (node->children[i]->path.size() == 1 && node->children[i]->path[0] == suffix)) {
                        exists = true;
                        break;
                    }
                }
            }
        }
        if (!exists) {
            if (target.isEmpty() || target.toString() == "") {
                return Path(suffix);
            }
            String baseStr = target.toString();
            if (baseStr.includes("/")) {
                if (baseStr.endsWith("/")) return Path(baseStr + suffix);
                return Path(baseStr + "/" + suffix);
            } else {
                return Path(baseStr + "." + suffix);
            }
        }
        idx++;
    }
}

static void collectQuery(const Tree<void>* node, const Path &target, bool strict, Array<Tree<void>*> &out) {
    for (usz i = 0; i < node->children.size(); ++i) {
        Tree<void>* child = node->children[i];
        if (target.common(child->path) || (!strict && isPrefixOf(child->path, target))) {
            out.push(child);
        }
        collectQuery(child, target, strict, out);
    }
}

Array<Tree<void>*> Tree<void>::queryAll(const Path &target, bool strict) const {
    Array<Tree<void>*> out;
    collectQuery(this, target, strict, out);
    return out;
}

Array<Tree<void>*> Tree<void>::queryAll(const String &target, bool strict) const {
    return queryAll(Path(target), strict);
}

Array<Tree<void>*> Tree<void>::queryAll(const char *target, bool strict) const {
    return queryAll(Path(target), strict);
}

Tree<void>* Tree<void>::query(const Path &target, bool strict) const {
    Array<Tree<void>*> res = queryAll(target, strict);
    if (res.isEmpty()) return nullptr;
    for (usz i = 0; i < res.size(); ++i) {
        if (res[i]->path == target) return res[i];
    }
    usz bestIdx = 0;
    int bestScore = -1;
    for (usz i = 0; i < res.size(); ++i) {
        int score = 0;
        for (usz k = 0; k < res[i]->path.parts.size(); ++k) {
            const String &pt = res[i]->path.parts[k];
            if (pt != "*" && pt != "**" && !pt.isEmpty()) score++;
        }
        if (score > bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }
    return res[bestIdx];
}

Tree<void>* Tree<void>::query(const String &target, bool strict) const {
    return query(Path(target), strict);
}

Tree<void>* Tree<void>::query(const char *target, bool strict) const {
    return query(Path(target), strict);
}

// -----------------------------------------------------------------------------
// JSON Stringifier (compact when identation == 0)
// -----------------------------------------------------------------------------

static String escapeJSON(const String &s) {
    String out = "\"";
    for (usz i = 0; i < s.size(); ++i) {
        char c = (char)s[i];
        if (c == '\"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if ((unsigned char)c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
            out += buf;
        } else {
            out.push((u8)c);
        }
    }
    out += "\"";
    return out;
}

static void emitIdentSpaces(String &out, u32 count) {
    for (u32 i = 0; i < count; ++i) out.push(' ');
}

static void stringifyJSONNode(const Tree<void> &node, u32 indentSize, u32 currentIndent, String &out) {
    // Check if this is a leaf node with value
    if (node.hasValue()) {
        int vt = node.valueType();
        if (vt == (int)TreeValueType::Byte) {
            out += node.asBool() ? "true" : "false";
        } else if (vt == (int)TreeValueType::U8 || vt == (int)TreeValueType::U32 || vt == (int)TreeValueType::U64) {
            out += String(node.asUint());
        } else if (vt == (int)TreeValueType::I32 || vt == (int)TreeValueType::I64) {
            out += String(node.asInt());
        } else if (vt == (int)TreeValueType::F32 || vt == (int)TreeValueType::F64) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", node.asNumber());
            out += buf;
        } else if (vt == (int)TreeValueType::Comment) {
            if (indentSize > 0) {
                out += "// " + node.asString();
            }
        } else if (vt == (int)TreeValueType::String) {
            out += escapeJSON(node.asString());
        } else if (vt == (int)TreeValueType::Custom) {
            // Custom typed nodes have "typ" and "val" children, handled below if children exist
            // If treated as leaf, emit object
            out += "{\"typ\":" + escapeJSON(node.get("typ") ? node.get("typ")->asString() : "")
                 + ",\"val\":" + escapeJSON(node.get("val") ? node.get("val")->asString() : "") + "}";
        }
        return;
    }

    // Array serialization
    if (node.isArray()) {
        Array<Tree<void>*> items;
        for (usz i = 0; i < node.children.size(); ++i) {
            if (indentSize == 0 && node.children[i]->valueType() == (int)TreeValueType::Comment) {
                continue; // Skip comments in compact mode
            }
            items.push(node.children[i]);
        }
        if (items.isEmpty()) {
            out += "[]";
            return;
        }
        out += "[";
        if (indentSize > 0) out += "\n";
        for (usz i = 0; i < items.size(); ++i) {
            if (indentSize > 0) emitIdentSpaces(out, currentIndent + indentSize);
            stringifyJSONNode(*items[i], indentSize, currentIndent + indentSize, out);
            if (i + 1 < items.size()) out += ",";
            if (indentSize > 0) out += "\n";
        }
        if (indentSize > 0) emitIdentSpaces(out, currentIndent);
        out += "]";
        return;
    }

    // Object serialization
    Array<Tree<void>*> fields;
    for (usz i = 0; i < node.children.size(); ++i) {
        if (indentSize == 0 && node.children[i]->valueType() == (int)TreeValueType::Comment) {
            continue;
        }
        fields.push(node.children[i]);
    }
    if (fields.isEmpty()) {
        out += "{}";
        return;
    }
    out += "{";
    if (indentSize > 0) out += "\n";
    for (usz i = 0; i < fields.size(); ++i) {
        Tree<void>* f = fields[i];
        if (f->valueType() == (int)TreeValueType::Comment && indentSize > 0) {
            emitIdentSpaces(out, currentIndent + indentSize);
            out += "// " + f->asString() + "\n";
            continue;
        }
        if (indentSize > 0) emitIdentSpaces(out, currentIndent + indentSize);
        out += escapeJSON(f->path.toString()) + (indentSize > 0 ? ": " : ":");
        stringifyJSONNode(*f, indentSize, currentIndent + indentSize, out);
        if (i + 1 < fields.size()) out += ",";
        if (indentSize > 0) out += "\n";
    }
    if (indentSize > 0) emitIdentSpaces(out, currentIndent);
    out += "}";
}

String stringifyJSON(const Tree<void> &tree, u32 identation) {
    String out;
    stringifyJSONNode(tree, identation, 0, out);
    return out;
}

// -----------------------------------------------------------------------------
// YAML Stringifier
// -----------------------------------------------------------------------------

static bool needsYamlQuotes(const String &s) {
    if (s.isEmpty()) return true;
    if (s == "true" || s == "false" || s == "null" || s == "~" || s == "yes" || s == "no") return true;
    char first = (char)s[0];
    if (first == '-' || first == '?' || first == ':' || first == ',' || first == '[' || first == ']' ||
        first == '{' || first == '}' || first == '#' || first == '&' || first == '*' || first == '!' ||
        first == '|' || first == '>' || first == '\'' || first == '\"' || first == '%' || first == '@') {
        return true;
    }
    for (usz i = 0; i < s.size(); ++i) {
        char c = (char)s[i];
        if (c == ':' || c == '#' || c == '\n' || c == '\r' || c == '\t' || c == '\"' || c == '\'') return true;
    }
    return false;
}

static void stringifyYamlNode(const Tree<void> &node, u32 indentStep, u32 currentIndent, bool firstLineNoIndent, String &out) {
    if (node.hasValue()) {
        int vt = node.valueType();
        if (vt == (int)TreeValueType::Byte) {
            out += node.asBool() ? "true\n" : "false\n";
        } else if (vt == (int)TreeValueType::U8 || vt == (int)TreeValueType::U32 || vt == (int)TreeValueType::U64) {
            out += String(node.asUint()) + "\n";
        } else if (vt == (int)TreeValueType::I32 || vt == (int)TreeValueType::I64) {
            out += String(node.asInt()) + "\n";
        } else if (vt == (int)TreeValueType::F32 || vt == (int)TreeValueType::F64) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g\n", node.asNumber());
            out += buf;
        } else if (vt == (int)TreeValueType::Comment) {
            out += "# " + node.asString() + "\n";
        } else if (vt == (int)TreeValueType::String) {
            String s = node.asString();
            if (needsYamlQuotes(s)) {
                out += escapeJSON(s) + "\n";
            } else {
                out += s + "\n";
            }
        } else {
            out += escapeJSON(node.asString()) + "\n";
        }
        return;
    }

    if (node.isArray()) {
        if (node.children.isEmpty()) {
            out += "[]\n";
            return;
        }
        for (usz i = 0; i < node.children.size(); ++i) {
            Tree<void>* c = node.children[i];
            if (c->valueType() == (int)TreeValueType::Comment) {
                emitIdentSpaces(out, currentIndent);
                out += "# " + c->asString() + "\n";
                continue;
            }
            emitIdentSpaces(out, currentIndent);
            out += "- ";
            stringifyYamlNode(*c, indentStep, currentIndent + indentStep, true, out);
        }
        return;
    }

    // Object
    if (node.children.isEmpty()) {
        out += "{}\n";
        return;
    }
    for (usz i = 0; i < node.children.size(); ++i) {
        Tree<void>* c = node.children[i];
        if (c->valueType() == (int)TreeValueType::Comment) {
            emitIdentSpaces(out, currentIndent);
            out += "# " + c->asString() + "\n";
            continue;
        }
        if (!firstLineNoIndent || i > 0) {
            emitIdentSpaces(out, currentIndent);
        }
        firstLineNoIndent = false;
        String k = c->path.toString();
        if (needsYamlQuotes(k)) out += escapeJSON(k);
        else out += k;
        out += ":";

        if (c->hasValue() && c->children.isEmpty()) {
            out += " ";
            stringifyYamlNode(*c, indentStep, currentIndent + indentStep, true, out);
        } else if (c->children.isEmpty() && !c->hasValue()) {
            out += " {}\n";
        } else {
            out += "\n";
            stringifyYamlNode(*c, indentStep, currentIndent + indentStep, false, out);
        }
    }
}

String stringifyYaml(const Tree<void> &tree, u32 identation) {
    String out;
    stringifyYamlNode(tree, identation, 0, false, out);
    return out;
}

// -----------------------------------------------------------------------------
// Unified YAML & JSON Parser with Comments & Go-Tos (Anchors/Aliases/Merge)
// -----------------------------------------------------------------------------

struct YamlJsonParser {
    const String &input;
    usz i = 0;
    int currentIndent = 0;
    bool isNewLine = true;
    Map<String, Tree<void>*> anchors;

    YamlJsonParser(const String &s) : input(s) {}

    static inline bool isWhitespace(char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    void skipSpace() {
        while (i < input.length()) {
            char c = (char)input[i];
            if (c == ' ') {
                if (isNewLine) currentIndent++;
                i++;
            } else if (c == '\t') {
                if (isNewLine) currentIndent += 4;
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

    void skipComments(Tree<void>* parent = nullptr) {
        while (i < input.length()) {
            skipSpace();
            if (i >= input.length()) break;
            char c1 = (char)input[i];
            char c2 = (i + 1 < input.length()) ? (char)input[i + 1] : '\0';

            if (c1 == '#') {
                i++;
                String comm;
                while (i < input.length() && input[i] != '\n' && input[i] != '\r') {
                    comm.push(input[i++]);
                }
                if (parent) {
                    parent->addChild(new Tree<Comment>(comm.trim()));
                }
                isNewLine = true;
                currentIndent = 0;
            } else if (c1 == '/' && c2 == '/') {
                i += 2;
                String comm;
                while (i < input.length() && input[i] != '\n' && input[i] != '\r') {
                    comm.push(input[i++]);
                }
                if (parent) {
                    parent->addChild(new Tree<Comment>(comm.trim()));
                }
                isNewLine = true;
                currentIndent = 0;
            } else if (c1 == '/' && c2 == '*') {
                i += 2;
                String comm;
                while (i + 1 < input.length() && !(input[i] == '*' && input[i + 1] == '/')) {
                    comm.push(input[i++]);
                }
                i += 2; // skip */
                if (parent) {
                    parent->addChild(new Tree<Comment>(comm.trim()));
                }
            } else {
                break;
            }
        }
    }

    String parseQuotedString(char quote) {
        String res;
        i++; // skip opening quote
        while (i < input.length()) {
            char c = (char)input[i];
            if (c == quote) {
                i++;
                break;
            }
            if (c == '\\' && i + 1 < input.length()) {
                i++;
                char esc = (char)input[i];
                if (esc == 'n') res.push('\n');
                else if (esc == 'r') res.push('\r');
                else if (esc == 't') res.push('\t');
                else if (esc == '\"') res.push('\"');
                else if (esc == '\'') res.push('\'');
                else if (esc == '\\') res.push('\\');
                else if (esc == 'u' && i + 4 < input.length()) {
                    // Simple 4-hex digit unicode decode
                    char hex[5] = { (char)input[i+1], (char)input[i+2], (char)input[i+3], (char)input[i+4], 0 };
                    unsigned int code = 0;
                    sscanf(hex, "%x", &code);
                    i += 4;
                    if (code < 0x80) res.push((u8)code);
                    else res.push('?');
                } else {
                    res.push((u8)esc);
                }
            } else {
                res.push((u8)c);
            }
            i++;
        }
        return res;
    }

    String parseScalar() {
        String res;
        while (i < input.length()) {
            char c = (char)input[i];
            if (c == ':' || c == ',' || c == ']' || c == '}' || c == '#' ||
                c == '\n' || c == '\r' || (c == '/' && i + 1 < input.length() && (input[i+1] == '/' || input[i+1] == '*'))) {
                break;
            }
            res.push((u8)c);
            i++;
        }
        return res.trim();
    }

    Tree<void>* parseValue(int parentIndent, Tree<void>* parentBranch = nullptr) {
        while (true) {
            skipComments(parentBranch);
            if (i >= input.length()) return nullptr;

            // Directives (%YAML, %TAG)
            if (currentIndent == 0 && input[i] == '%' &&
                (input.indexOf("%YAML", i) == (long long)i || input.indexOf("%TAG", i) == (long long)i)) {
                while (i < input.length() && input[i] != '\n') i++;
                isNewLine = true;
                currentIndent = 0;
                continue;
            }

            // Document Separators (---)
            if (input[i] == '-' && i + 2 < input.length() && input[i + 1] == '-' && input[i + 2] == '-') {
                i += 3;
                while (i < input.length() && input[i] != '\n') i++;
                isNewLine = true;
                currentIndent = 0;
                continue;
            }
            break;
        }

        if (i >= input.length()) return nullptr;

        // Anchor definition (&anchorName node)
        if (input[i] == '&') {
            i++;
            String anchorName;
            while (i < input.length() && !isWhitespace((char)input[i])) {
                anchorName.push(input[i++]);
            }
            Tree<void>* val = parseValue(parentIndent, parentBranch);
            if (val) {
                anchors.put(anchorName, val);
            }
            return val;
        }

        // Alias reference (*aliasName) - Go-To
        if (input[i] == '*') {
            i++;
            String aliasName;
            while (i < input.length() && !isWhitespace((char)input[i]) && input[i] != ',' && input[i] != ']' && input[i] != '}') {
                aliasName.push(input[i++]);
            }
            Tree<void>** ref = anchors.get(aliasName);
            if (ref && *ref) {
                return (*ref)->clone();
            }
            return new Tree<void>();
        }

        int blockIndent = currentIndent;

        // Flow Sequence [...]
        if (input[i] == '[') {
            i++;
            Tree<void>* arr = new Tree<void>();
            arr->setArray(true);
            while (i < input.length()) {
                skipComments(arr);
                if (i < input.length() && input[i] == ']') {
                    i++;
                    break;
                }
                Tree<void>* val = parseValue(blockIndent, arr);
                if (val) arr->addChild(val);
                skipSpace();
                if (i < input.length() && input[i] == ',') i++;
            }
            return arr;
        }

        // Flow Mapping {...}
        if (input[i] == '{') {
            i++;
            Tree<void>* mapNode = new Tree<void>();
            while (i < input.length()) {
                skipComments(mapNode);
                if (i < input.length() && input[i] == '}') {
                    i++;
                    break;
                }
                String k;
                skipSpace();
                if (i < input.length() && (input[i] == '\"' || input[i] == '\'')) {
                    k = parseQuotedString((char)input[i]);
                } else {
                    k = parseScalar();
                }
                skipSpace();
                if (i < input.length() && input[i] == ':') i++;
                Tree<void>* val = parseValue(blockIndent, mapNode);
                if (val) {
                    if (k == "<<" && !val->hasValue()) {
                        // Merge key (Go-To anchor merge)
                        for (usz m = 0; m < val->children.size(); ++m) {
                            mapNode->addChild(val->children[m]->clone());
                        }
                        delete val;
                    } else {
                        val->path = k;
                        mapNode->addChild(val);
                    }
                }
                skipSpace();
                if (i < input.length() && input[i] == ',') i++;
            }
            return mapNode;
        }

        // Block Sequence (- item)
        if (input[i] == '-' && (i + 1 < input.length() && isWhitespace((char)input[i + 1]))) {
            Tree<void>* arr = new Tree<void>();
            arr->setArray(true);
            int seqIndent = currentIndent;
            while (i < input.length()) {
                skipComments(arr);
                if (currentIndent < seqIndent) break;
                if (i >= input.length() || input[i] != '-') break;
                i++; // skip '-'
                skipSpace();
                Tree<void>* val = parseValue(seqIndent + 1, arr);
                if (val) arr->addChild(val);
                skipComments(arr);
            }
            return arr;
        }

        // Lookahead to check if this is a key-value mapping
        bool isKey = false;
        usz peek = i;
        if (peek < input.length() && (input[peek] == '\"' || input[peek] == '\'')) {
            char q = (char)input[peek++];
            while (peek < input.length()) {
                if (input[peek] == '\\' && peek + 1 < input.length()) {
                    peek += 2;
                    continue;
                }
                if (input[peek] == q) {
                    peek++;
                    break;
                }
                peek++;
            }
            while (peek < input.length() && (input[peek] == ' ' || input[peek] == '\t')) peek++;
            if (peek < input.length() && input[peek] == ':' &&
                (peek + 1 >= input.length() || isWhitespace((char)input[peek + 1]))) {
                isKey = true;
            }
        } else {
            while (peek < input.length()) {
                char cp = (char)input[peek];
                if (cp == '\n' || cp == '\r') break;
                if (cp == ':' && (peek + 1 >= input.length() || isWhitespace((char)input[peek + 1]))) {
                    isKey = true;
                    break;
                }
                peek++;
            }
        }

        if (isKey) {
            Tree<void>* branch = new Tree<void>();
            while (i < input.length()) {
                skipComments(branch);
                if (currentIndent < blockIndent) break;
                if (i >= input.length()) break;
                if (input[i] == '-' && (i + 1 >= input.length() || isWhitespace((char)input[i + 1]))) break;

                String k;
                if (input[i] == '\"' || input[i] == '\'') {
                    k = parseQuotedString((char)input[i]);
                    skipSpace();
                    if (i < input.length() && input[i] == ':') i++;
                } else {
                    while (i < input.length()) {
                        char ck = (char)input[i];
                        if (ck == '\n' || ck == '\r') break;
                        if (ck == ':' && (i + 1 >= input.length() || isWhitespace((char)input[i + 1]))) {
                            i++;
                            break;
                        }
                        k.push((u8)ck);
                        i++;
                    }
                }
                k = k.trim();
                if (k.isEmpty()) break;

                Tree<void>* v = parseValue(blockIndent, branch);
                if (v) {
                    if (k == "<<" && !v->hasValue()) {
                        // Merge key (Go-To anchor merge)
                        for (usz m = 0; m < v->children.size(); ++m) {
                            branch->addChild(v->children[m]->clone());
                        }
                        delete v;
                    } else {
                        v->path = k;
                        branch->addChild(v);
                    }
                }
                skipComments(branch);
                if (currentIndent < blockIndent) break;
            }
            return branch;
        }

        // Scalar parsing
        String s;
        if (input[i] == '\"' || input[i] == '\'') {
            s = parseQuotedString((char)input[i]);
            return new Tree<String>(s);
        } else {
            s = parseScalar();
        }

        if (s == "true") return new Tree<bool>(true);
        if (s == "false") return new Tree<bool>(false);
        if (s == "null" || s == "~") return new Tree<void>();

        // Numeric checking
        bool isNum = true;
        int dotCount = 0;
        for (usz k = 0; k < s.length(); ++k) {
            char ch = (char)s[k];
            if (ch == '.') dotCount++;
            else if (ch < '0' || ch > '9') {
                if (k != 0 || ch != '-') isNum = false;
            }
        }
        if (isNum && s.length() > 0 && s != "-" && dotCount <= 1) {
            if (dotCount == 1) return new Tree<f64>(s.toDouble());
            return new Tree<i64>(s.toInt());
        }

        return new Tree<String>(s);
    }
};

// Post-processor for custom types
static Tree<void>* resolveCustomTypes(Tree<void>* node, const Array<Typ> &th) {
    if (!node) return nullptr;

    for (usz i = 0; i < node->children.size(); ++i) {
        node->children[i] = resolveCustomTypes(node->children[i], th);
    }

    // Check if node is an object with "typ" and "val"
    Tree<void>* typNode = node->get("typ");
    Tree<void>* valNode = node->get("val");
    if (typNode && valNode && node->children.size() == 2) {
        String typName = typNode->asString();
        String valStr = valNode->asString();
        const Typ* match = nullptr;
        for (usz k = 0; k < th.size(); ++k) {
            if (th[k].name == typName) {
                match = &th[k];
                break;
            }
        }
        if (match && match->instantiate) {
            Tree<void>* customNode = match->instantiate(valStr);
            if (customNode) {
                customNode->path = node->path;
                delete node;
                return customNode;
            }
        } else {
            // "if the type isnt there with customTs, it will just handle it as String."
            Tree<void>* strNode = new Tree<String>(node->path, valStr);
            delete node;
            return strNode;
        }
    }
    return node;
}

bool parseTreeWithTyps(const Array<Typ> &th, const String &string, Tree<void> &tree) {
    tree.clear();
    YamlJsonParser p(string);
    Tree<void>* parsed = p.parseValue(-1, &tree);
    if (!parsed) return false;

    parsed = resolveCustomTypes(parsed, th);
    if (!parsed) return false;

    if (!parsed->hasValue()) {
        tree.setArray(parsed->isArray());
        for (usz i = 0; i < parsed->children.size(); ++i) {
            tree.addChild(parsed->children[i]->clone());
        }
        delete parsed;
    } else {
        tree.addChild(parsed);
    }
    return true;
}

// -----------------------------------------------------------------------------
// Petrified Tree (The most bitpacked universally possible serialization)
// -----------------------------------------------------------------------------

static u64 zigzagEncode(i64 n) {
    return (u64)((n << 1) ^ (n >> 63));
}

static i64 zigzagDecode(u64 n) {
    return (i64)((n >> 1) ^ -(i64)(n & 1));
}

static void collectSymbols(const Tree<void>* node, Array<String> &dict, Map<String, usz> &dictMap) {
    if (!node) return;

    if (!node->path.isEmpty() && node->path.toString().length() > 0) {
        String p = node->path.toString();
        if (!dictMap.has(p)) {
            dictMap.put(p, dict.size());
            dict.push(p);
        }
    }

    if (node->valueType() == (int)TreeValueType::Custom) {
        if (auto* typNode = node->get("typ")) {
            String t = typNode->asString();
            if (!dictMap.has(t)) {
                dictMap.put(t, dict.size());
                dict.push(t);
            }
        }
    } else if (node->valueType() == (int)TreeValueType::String) {
        String s = node->asString();
        if (s.length() > 0 && !dictMap.has(s)) {
            dictMap.put(s, dict.size());
            dict.push(s);
        }
    }

    for (usz i = 0; i < node->children.size(); ++i) {
        collectSymbols(node->children[i], dict, dictMap);
    }
}

static void petrifyNode(const Tree<void>* node, const Map<String, usz> &dictMap, String &out) {
    if (!node) {
        out.push(0x00); // Void
        return;
    }

    // Array (Tag 13 = 0xD)
    if (node->isArray()) {
        usz count = node->children.size();
        u8 payload = count < 15 ? (u8)count : 15;
        out.push((13 << 4) | payload);
        if (count >= 15) {
            out.pushVarLong((long long)count);
        }
        for (usz i = 0; i < count; ++i) {
            petrifyNode(node->children[i], dictMap, out);
        }
        return;
    }

    // Object (Tag 14 = 0xE)
    if (node->isObject() && node->children.size() > 0 && node->valueType() != (int)TreeValueType::Custom) {
        usz count = node->children.size();
        u8 payload = count < 15 ? (u8)count : 15;
        out.push((14 << 4) | payload);
        if (count >= 15) {
            out.pushVarLong((long long)count);
        }

        Array<Tree<void>*> sortedChildren = node->children;
        for (usz i = 0; i < sortedChildren.size(); ++i) {
            for (usz j = i + 1; j < sortedChildren.size(); ++j) {
                if (sortedChildren[j]->path.toString() < sortedChildren[i]->path.toString()) {
                    Tree<void>* tmp = sortedChildren[i];
                    sortedChildren[i] = sortedChildren[j];
                    sortedChildren[j] = tmp;
                }
            }
        }

        for (usz i = 0; i < count; ++i) {
            Tree<void>* child = sortedChildren[i];
            String k = child->path.toString();
            const usz* kidx = dictMap.get(k);
            usz keyIdx = kidx ? *kidx : 0;
            out.pushVarLong((long long)keyIdx);
            petrifyNode(child, dictMap, out);
        }
        return;
    }

    // Leaf Values
    int vt = node->valueType();
    switch (vt) {
        case (int)TreeValueType::Void:
            out.push(0x00);
            break;

        case (int)TreeValueType::Byte:
            out.push(node->asBool() ? 0x21 : 0x10);
            break;

        case (int)TreeValueType::U8: {
            u8 v = node->as<u8>();
            if (v <= 15) {
                out.push((3 << 4) | v); // Tiny int
            } else {
                out.push(0x40);
                out.push(v);
            }
            break;
        }

        case (int)TreeValueType::U32: {
            u32 v = node->as<u32>();
            if (v <= 15) {
                out.push((3 << 4) | (u8)v);
            } else {
                out.push(0x50);
                out.pushVarLong((long long)v);
            }
            break;
        }

        case (int)TreeValueType::U64: {
            u64 v = node->as<u64>();
            if (v <= 15) {
                out.push((3 << 4) | (u8)v);
            } else {
                out.push(0x51);
                out.pushVarLong((long long)v);
            }
            break;
        }

        case (int)TreeValueType::I32: {
            i32 v = node->as<i32>();
            if (v >= 0 && v <= 15) {
                out.push((3 << 4) | (u8)v);
            } else {
                out.push(0x60);
                out.pushVarLong((long long)zigzagEncode((i64)v));
            }
            break;
        }

        case (int)TreeValueType::I64: {
            i64 v = node->as<i64>();
            if (v >= 0 && v <= 15) {
                out.push((3 << 4) | (u8)v);
            } else {
                out.push(0x61);
                out.pushVarLong((long long)zigzagEncode(v));
            }
            break;
        }

        case (int)TreeValueType::F32: {
            f32 v = node->as<f32>();
            out.push(0x70);
            u8 bytes[4];
            memcpy(bytes, &v, 4);
            for (int k = 0; k < 4; ++k) out.push(bytes[k]);
            break;
        }

        case (int)TreeValueType::F64: {
            f64 v = node->as<f64>();
            out.push(0x80);
            u8 bytes[8];
            memcpy(bytes, &v, 8);
            for (int k = 0; k < 8; ++k) out.push(bytes[k]);
            break;
        }

        case (int)TreeValueType::Comment: {
            String s = node->asString();
            usz len = s.length();
            u8 payload = len < 15 ? (u8)len : 15;
            out.push((11 << 4) | payload);
            if (len >= 15) {
                out.pushVarLong((long long)len);
            }
            out += s;
            break;
        }

        case (int)TreeValueType::String: {
            String s = node->asString();
            const usz* didx = dictMap.get(s);
            if (didx) {
                usz idx = *didx;
                u8 payload = idx < 15 ? (u8)idx : 15;
                out.push((10 << 4) | payload); // Dict ref
                if (idx >= 15) {
                    out.pushVarLong((long long)idx);
                }
            } else {
                usz len = s.length();
                u8 payload = len < 15 ? (u8)len : 15;
                out.push((9 << 4) | payload); // Inline string
                if (len >= 15) {
                    out.pushVarLong((long long)len);
                }
                out += s;
            }
            break;
        }

        case (int)TreeValueType::Custom: {
            String typName = node->get("typ") ? node->get("typ")->asString() : "";
            String valStr = node->get("val") ? node->get("val")->asString() : "";
            const usz* tidx = dictMap.get(typName);
            usz typIdx = tidx ? *tidx : 0;
            u8 payload = typIdx < 15 ? (u8)typIdx : 15;
            out.push((12 << 4) | payload);
            if (typIdx >= 15) {
                out.pushVarLong((long long)typIdx);
            }
            out.pushVarLong((long long)valStr.length());
            out += valStr;
            break;
        }

        default:
            out.push(0x00);
            break;
    }
}

String petrify(const Tree<void> &tree) {
    Array<String> dict;
    Map<String, usz> dictMap;
    collectSymbols(&tree, dict, dictMap);

    // Sort dictionary symbols canonically
    for (usz i = 0; i < dict.size(); ++i) {
        for (usz j = i + 1; j < dict.size(); ++j) {
            if (dict[j] < dict[i]) {
                String tmp = dict[i];
                dict[i] = dict[j];
                dict[j] = tmp;
            }
        }
    }
    dictMap.clear();
    for (usz i = 0; i < dict.size(); ++i) {
        dictMap.put(dict[i], i);
    }

    String out;
    out.pushVarLong((long long)dict.size());
    for (usz i = 0; i < dict.size(); ++i) {
        out.pushVarLong((long long)dict[i].length());
        out += dict[i];
    }

    petrifyNode(&tree, dictMap, out);
    return out;
}

static Tree<void>* hydrateNode(const String &petrified, usz &offset, const Array<String> &dict) {
    if (offset >= petrified.size()) return nullptr;

    u8 tag = (u8)petrified[offset++];
    u8 kind = (tag >> 4) & 0x0F;
    u8 payload = tag & 0x0F;

    switch (kind) {
        case 0: // Void
            return new Tree<void>();

        case 1: // Bool False
            return new Tree<bool>(false);

        case 2: // Bool True
            return new Tree<bool>(true);

        case 3: // Tiny Int
            return new Tree<i32>((i32)payload);

        case 4: { // U8
            if (offset >= petrified.size()) return nullptr;
            u8 v = (u8)petrified[offset++];
            return new Tree<u8>(v);
        }

        case 5: { // VarInt Unsigned (u32 or u64)
            auto vr = petrified.peekVarLong(offset);
            if (vr.error) return nullptr;
            offset += vr.bytes;
            if (payload == 0) return new Tree<u32>((u32)vr.value);
            return new Tree<u64>((u64)vr.value);
        }

        case 6: { // VarInt Signed (i32 or i64)
            auto vr = petrified.peekVarLong(offset);
            if (vr.error) return nullptr;
            offset += vr.bytes;
            i64 val = zigzagDecode((u64)vr.value);
            if (payload == 0) return new Tree<i32>((i32)val);
            return new Tree<i64>(val);
        }

        case 7: { // F32
            if (offset + 4 > petrified.size()) return nullptr;
            f32 f;
            memcpy(&f, petrified.data() + offset, 4);
            offset += 4;
            return new Tree<f32>(f);
        }

        case 8: { // F64
            if (offset + 8 > petrified.size()) return nullptr;
            f64 d;
            memcpy(&d, petrified.data() + offset, 8);
            offset += 8;
            return new Tree<f64>(d);
        }

        case 9: { // Inline String
            usz len = payload;
            if (payload == 15) {
                auto vr = petrified.peekVarLong(offset);
                if (vr.error) return nullptr;
                offset += vr.bytes;
                len = (usz)vr.value;
            }
            if (offset + len > petrified.size()) return nullptr;
            String s = petrified.substring(offset, offset + len);
            offset += len;
            return new Tree<String>(s);
        }

        case 10: { // Dict String
            usz idx = payload;
            if (payload == 15) {
                auto vr = petrified.peekVarLong(offset);
                if (vr.error) return nullptr;
                offset += vr.bytes;
                idx = (usz)vr.value;
            }
            if (idx >= dict.size()) return nullptr;
            return new Tree<String>(dict[idx]);
        }

        case 11: { // Comment
            usz len = payload;
            if (payload == 15) {
                auto vr = petrified.peekVarLong(offset);
                if (vr.error) return nullptr;
                offset += vr.bytes;
                len = (usz)vr.value;
            }
            if (offset + len > petrified.size()) return nullptr;
            String s = petrified.substring(offset, offset + len);
            offset += len;
            return new Tree<Comment>(Comment(s));
        }

        case 12: { // Custom Typ
            usz typIdx = payload;
            if (payload == 15) {
                auto vr = petrified.peekVarLong(offset);
                if (vr.error) return nullptr;
                offset += vr.bytes;
                typIdx = (usz)vr.value;
            }
            if (typIdx >= dict.size()) return nullptr;
            String typName = dict[typIdx];

            auto lenRes = petrified.peekVarLong(offset);
            if (lenRes.error) return nullptr;
            offset += lenRes.bytes;
            usz valLen = (usz)lenRes.value;
            if (offset + valLen > petrified.size()) return nullptr;
            String valStr = petrified.substring(offset, offset + valLen);
            offset += valLen;

            Tree<void>* customNode = new Tree<void>();
            customNode->addChild(new Tree<String>("typ", typName));
            customNode->addChild(new Tree<String>("val", valStr));
            return customNode;
        }

        case 13: { // Array
            usz count = payload;
            if (payload == 15) {
                auto vr = petrified.peekVarLong(offset);
                if (vr.error) return nullptr;
                offset += vr.bytes;
                count = (usz)vr.value;
            }
            Tree<void>* arr = new Tree<void>();
            arr->setArray(true);
            for (usz i = 0; i < count; ++i) {
                Tree<void>* c = hydrateNode(petrified, offset, dict);
                if (!c) {
                    delete arr;
                    return nullptr;
                }
                arr->addChild(c);
            }
            return arr;
        }

        case 14: { // Object
            usz count = payload;
            if (payload == 15) {
                auto vr = petrified.peekVarLong(offset);
                if (vr.error) return nullptr;
                offset += vr.bytes;
                count = (usz)vr.value;
            }
            Tree<void>* obj = new Tree<void>();
            for (usz i = 0; i < count; ++i) {
                auto kr = petrified.peekVarLong(offset);
                if (kr.error) {
                    delete obj;
                    return nullptr;
                }
                offset += kr.bytes;
                usz keyIdx = (usz)kr.value;
                if (keyIdx >= dict.size()) {
                    delete obj;
                    return nullptr;
                }
                Tree<void>* c = hydrateNode(petrified, offset, dict);
                if (!c) {
                    delete obj;
                    return nullptr;
                }
                c->path = dict[keyIdx];
                obj->addChild(c);
            }
            return obj;
        }

        default:
            return nullptr;
    }
}

bool hydrate(const String &petrified, Tree<void> &tree) {
    if (petrified.isEmpty()) return false;
    usz offset = 0;

    auto dictCountRes = petrified.peekVarLong(offset);
    if (dictCountRes.error) return false;
    offset += dictCountRes.bytes;
    usz dictCount = (usz)dictCountRes.value;

    Array<String> dict;
    for (usz i = 0; i < dictCount; ++i) {
        auto strLenRes = petrified.peekVarLong(offset);
        if (strLenRes.error) return false;
        offset += strLenRes.bytes;
        usz sLen = (usz)strLenRes.value;
        if (offset + sLen > petrified.size()) return false;
        dict.push(petrified.substring(offset, offset + sLen));
        offset += sLen;
    }

    Tree<void>* root = hydrateNode(petrified, offset, dict);
    if (!root) return false;

    tree.clear();
    tree.path = root->path;
    tree.setArray(root->isArray());

    if (!root->hasValue()) {
        for (usz i = 0; i < root->children.size(); ++i) {
            tree.addChild(root->children[i]->clone());
        }
        delete root;
    } else {
        tree.addChild(root);
    }
    return true;
}

// -----------------------------------------------------------------------------
// Core serialization bridge methods on Tree<void>
// -----------------------------------------------------------------------------

String Tree<void>::toString() const {
    return petrify(*this);
}

String Tree<void>::toBinary() const {
    return petrify(*this);
}

bool Tree<void>::readFromBinary(const u8 *data, usz length, usz &bytesRead) {
    if (!data || length == 0) return false;
    String s(data, length);
    bool ok = hydrate(s, *this);
    if (ok) bytesRead = length;
    return ok;
}

Tree<void> Tree<void>::parse(const String &val) {
    Tree<void> t;
    if (!hydrate(val, t)) {
        parseTree(val, t);
    }
    return t;
}

Array<Tree<void>> Tree<void>::parseMany(const String &data) {
    Array<Tree<void>> list;
    if (data.isEmpty()) return list;

    usz offset = 0;
    while (offset < data.size()) {
        usz currentStart = offset;
        auto dictCountRes = data.peekVarLong(offset);
        if (dictCountRes.error) break;
        offset += dictCountRes.bytes;
        usz dictCount = (usz)dictCountRes.value;

        Array<String> dict;
        bool ok = true;
        for (usz i = 0; i < dictCount; ++i) {
            auto strLenRes = data.peekVarLong(offset);
            if (strLenRes.error) { ok = false; break; }
            offset += strLenRes.bytes;
            usz sLen = (usz)strLenRes.value;
            if (offset + sLen > data.size()) { ok = false; break; }
            dict.push(data.substring(offset, offset + sLen));
            offset += sLen;
        }
        if (!ok) break;

        Tree<void>* root = hydrateNode(data, offset, dict);
        if (!root) break;

        Tree<void> item;
        item.path = root->path;
        item.setArray(root->isArray());
        if (!root->hasValue()) {
            for (usz i = 0; i < root->children.size(); ++i) {
                item.addChild(root->children[i]->clone());
            }
            delete root;
        } else {
            item.addChild(root);
        }
        list.push(std::move(item));
        if (offset <= currentStart) break;
    }
    return list;
}

} // namespace Ksee
