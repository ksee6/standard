/**
 * @file Tree.cpp
 * @brief Radix Trie tree data structure implementation for Ksee.
 */

#include "../../include/Ksee/Tree.hpp"

namespace Ksee {

Tree<void>::Tree() {}

Tree<void>::Tree(const Path &p) : path(p) {}

Tree<void>::Tree(const String &p) {
    if (p.size() > 0) {
        auto vl = p.peekVarLong(0);
        if (!vl.error && vl.bytes > 0 && (usz)vl.bytes + (usz)vl.value == p.size()) {
            usz bytesRead = 0;
            if (readFromBinary(reinterpret_cast<const u8*>(p.data()) + vl.bytes, (usz)vl.value, bytesRead)) {
                return;
            }
            clear();
        }
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
}

Tree<void>::Tree(const Tree<void> &other) : path(other.path) {
    for (usz i = 0; i < other.children.size(); ++i) {
        Tree<void>* c = other.children[i]->clone();
        c->parent = this;
        children.push(c);
    }
}

Tree<void>::Tree(Tree<void> &&other) noexcept
    : children(Move(other.children)), parent(other.parent), path(Move(other.path)) {
    other.parent = nullptr;
    for (usz i = 0; i < children.size(); ++i) {
        children[i]->parent = this;
    }
}

Tree<void>& Tree<void>::operator=(const Tree<void> &other) {
    if (this != &other) {
        clear();
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
        children = Move(other.children);
        parent = other.parent;
        path = Move(other.path);
        other.parent = nullptr;
        for (usz i = 0; i < children.size(); ++i) {
            children[i]->parent = this;
        }
    }
    return *this;
}

Tree<void>* Tree<void>::clone() const {
    Tree<void>* n = new Tree<void>(path);
    for (usz i = 0; i < children.size(); ++i) {
        Tree<void>* c = children[i]->clone();
        c->parent = n;
        n->children.push(c);
    }
    return n;
}

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

Tree<void>* Tree<void>::push(const String &target) {
    return push(Path(target));
}

Tree<void>* Tree<void>::push(const char *target) {
    return push(Path(target));
}

void collectQuery(const Tree<void>* node, const Path &target, bool strict, Array<Tree<void>*> &out) {
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

static void encodeVarLong(String &buf, u64 v) {
    do {
        u8 t = v & 0x7F;
        v >>= 7;
        if (v) t |= 0x80;
        buf.push(t);
    } while (v);
}

static bool decodeVarLong(const u8 *data, usz len, usz &pos, u64 &out) {
    u64 r = 0;
    int s = 0;
    bool ok = false;
    while (pos < len) {
        u8 b = data[pos++];
        r |= (u64)(b & 0x7F) << s;
        s += 7;
        if (!(b & 0x80)) { ok = true; break; }
        if (s >= 70) return false;
    }
    if (!ok) return false;
    out = r;
    return true;
}

static bool compareTreeNodes(const Tree<void>* a, const Tree<void>* b) {
    String pa = a->path.toString();
    String pb = b->path.toString();
    if (pa != pb) return pa < pb;
    return a->asString() < b->asString();
}

static void serializeNode(const Tree<void>* node, String &buf) {
    // 1. Path
    String pStr = node->path.toString();
    encodeVarLong(buf, pStr.size());
    if (pStr.size() > 0) {
        buf.append(pStr.data(), pStr.size());
    }

    // 2. Value type and value
    if (auto s = dynamic_cast<const Tree<String>*>(node)) {
        buf.push((u8)1); // String
        encodeVarLong(buf, s->value.size());
        if (s->value.size() > 0) {
            buf.append(s->value.data(), s->value.size());
        }
    } else if (auto b = dynamic_cast<const Tree<bool>*>(node)) {
        buf.push((u8)2); // Bool
        buf.push(b->value ? (u8)1 : (u8)0);
    } else if (auto l = dynamic_cast<const Tree<long long>*>(node)) {
        buf.push((u8)3); // Int64
        long long v = l->value;
        u64 zz = (v >= 0) ? ((u64)v << 1) : (((u64)(~v) << 1) | 1);
        encodeVarLong(buf, zz);
    } else if (auto i = dynamic_cast<const Tree<int>*>(node)) {
        buf.push((u8)3); // Int64
        long long v = i->value;
        u64 zz = (v >= 0) ? ((u64)v << 1) : (((u64)(~v) << 1) | 1);
        encodeVarLong(buf, zz);
    } else if (auto u = dynamic_cast<const Tree<u64>*>(node)) {
        buf.push((u8)3); // Int64
        long long v = (long long)u->value;
        u64 zz = (v >= 0) ? ((u64)v << 1) : (((u64)(~v) << 1) | 1);
        encodeVarLong(buf, zz);
    } else if (auto uz = dynamic_cast<const Tree<usz>*>(node)) {
        buf.push((u8)3); // Int64
        long long v = (long long)uz->value;
        u64 zz = (v >= 0) ? ((u64)v << 1) : (((u64)(~v) << 1) | 1);
        encodeVarLong(buf, zz);
    } else if (auto f = dynamic_cast<const Tree<f64>*>(node)) {
        buf.push((u8)4); // Float64
        union { f64 d; u64 u; } un; un.d = f->value;
        for (int k = 0; k < 8; ++k) buf.push((u8)((un.u >> (k * 8)) & 0xFF));
    } else if (auto f3 = dynamic_cast<const Tree<f32>*>(node)) {
        buf.push((u8)4); // Float64
        union { f64 d; u64 u; } un; un.d = (f64)f3->value;
        for (int k = 0; k < 8; ++k) buf.push((u8)((un.u >> (k * 8)) & 0xFF));
    } else {
        buf.push((u8)0); // Void
    }

    // 3. Children (sorted deterministically)
    Array<Tree<void>*> sortedChildren = node->children;
    for (usz i = 1; i < sortedChildren.size(); ++i) {
        Tree<void>* key = sortedChildren[i];
        long long j = (long long)i - 1;
        while (j >= 0 && compareTreeNodes(key, sortedChildren[(usz)j])) {
            sortedChildren[(usz)(j + 1)] = sortedChildren[(usz)j];
            j--;
        }
        sortedChildren[(usz)(j + 1)] = key;
    }

    encodeVarLong(buf, sortedChildren.size());
    for (usz i = 0; i < sortedChildren.size(); ++i) {
        serializeNode(sortedChildren[i], buf);
    }
}

static Tree<void>* deserializeNode(const u8 *data, usz len, usz &pos) {
    // 1. Path
    u64 pathLen = 0;
    if (!decodeVarLong(data, len, pos, pathLen)) return nullptr;
    if (pos + pathLen > len) return nullptr;
    String pStr(reinterpret_cast<const u8*>(data + pos), (usz)pathLen);
    pos += (usz)pathLen;
    Path nodePath(pStr);

    // 2. Value type and value
    if (pos >= len) return nullptr;
    u8 tag = data[pos++];

    Tree<void>* node = nullptr;
    if (tag == 1) { // String
        u64 strLen = 0;
        if (!decodeVarLong(data, len, pos, strLen)) return nullptr;
        if (pos + strLen > len) return nullptr;
        String val(reinterpret_cast<const u8*>(data + pos), (usz)strLen);
        pos += (usz)strLen;
        node = new Tree<String>(nodePath, val);
    } else if (tag == 2) { // Bool
        if (pos >= len) return nullptr;
        bool b = (data[pos++] != 0);
        node = new Tree<bool>(nodePath, b);
    } else if (tag == 3) { // Int64
        u64 zz = 0;
        if (!decodeVarLong(data, len, pos, zz)) return nullptr;
        long long iv = (zz & 1) ? -(long long)((zz >> 1) + 1) : (long long)(zz >> 1);
        node = new Tree<long long>(nodePath, iv);
    } else if (tag == 4) { // Float64
        if (pos + 8 > len) return nullptr;
        union { f64 d; u64 u; } un; un.u = 0;
        for (int k = 0; k < 8; ++k) un.u |= ((u64)data[pos + k]) << (k * 8);
        pos += 8;
        node = new Tree<f64>(nodePath, un.d);
    } else if (tag == 0) { // Void
        node = new Tree<void>(nodePath);
    } else {
        return nullptr;
    }

    // 3. Children
    u64 childCount = 0;
    if (!decodeVarLong(data, len, pos, childCount)) {
        delete node;
        return nullptr;
    }

    for (u64 i = 0; i < childCount; ++i) {
        Tree<void>* child = deserializeNode(data, len, pos);
        if (!child) {
            delete node;
            return nullptr;
        }
        child->parent = node;
        node->children.push(child);
    }

    return node;
}

String Tree<void>::toBinary() const {
    String payload;
    serializeNode(this, payload);
    return payload;
}

bool Tree<void>::readFromBinary(const u8 *data, usz length, usz &bytesRead) {
    if (!data || length == 0) return false;

    usz pos = 0;
    Tree<void>* decodedRoot = deserializeNode(data, length, pos);
    if (!decodedRoot) return false;

    clear();
    path = decodedRoot->path;
    for (usz i = 0; i < decodedRoot->children.size(); ++i) {
        Tree<void>* ch = decodedRoot->children[i];
        ch->parent = this;
        children.push(ch);
    }
    decodedRoot->children.clear();
    delete decodedRoot;

    bytesRead = pos;
    return true;
}

String Tree<void>::toString() const {
    String payload = toBinary();
    String out;
    encodeVarLong(out, payload.size());
    out += payload;
    return out;
}

Tree<void> Tree<void>::parse(const String &data) {
    Tree<void> t;
    if (data.isEmpty()) return t;
    auto vl = data.peekVarLong(0);
    if (vl.error || vl.bytes <= 0) return t;
    usz payloadLen = (usz)vl.value;
    if ((usz)vl.bytes + payloadLen > data.size()) return t;
    usz bytesRead = 0;
    t.readFromBinary(reinterpret_cast<const u8*>(data.data()) + vl.bytes, payloadLen, bytesRead);
    return t;
}

Array<Tree<void>> Tree<void>::parseMany(const String &data) {
    Array<Tree<void>> list;
    usz offset = 0;
    while (offset < data.size()) {
        auto vl = data.peekVarLong(offset);
        if (vl.error || vl.bytes <= 0) break;
        usz payloadLen = (usz)vl.value;
        usz nextOffset = offset + (usz)vl.bytes + payloadLen;
        if (nextOffset > data.size()) break;

        Tree<void> t;
        usz bytesRead = 0;
        if (t.readFromBinary(reinterpret_cast<const u8*>(data.data()) + offset + vl.bytes, payloadLen, bytesRead)) {
            list.push(Ksee::Move(t));
        } else {
            break;
        }
        offset = nextOffset;
    }
    return list;
}

} // namespace Ksee

