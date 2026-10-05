/**
 * @file Tree.cpp
 * @brief Radix Trie tree data structure implementation for Ksee.
 */

#include "../../include/Ksee/Tree.hpp"

namespace Ksee {

Tree<void>::Tree() {}

Tree<void>::Tree(const Path &p) : path(p) {}

Tree<void>::Tree(const String &p) : path(Path(p)) {}

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

    for (usz i = 0; i < children.size(); ++i) {
        if (children[i]->path == target) {
            delete children[i];
            children.remove(i);
            return true;
        }
        if (isPrefixOf(children[i]->path, target) && !children[i]->path.isEmpty()) {
            Path remaining = slicePath(target, children[i]->path.parts.size());
            if (children[i]->deleteKey(remaining)) {
                if (children[i]->children.isEmpty() && children[i]->isNull()) {
                    delete children[i];
                    children.remove(i);
                }
                return true;
            }
        }
    }
    return false;
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
    return res.isEmpty() ? nullptr : res[0];
}

Tree<void>* Tree<void>::query(const String &target, bool strict) const {
    return query(Path(target), strict);
}

Tree<void>* Tree<void>::query(const char *target, bool strict) const {
    return query(Path(target), strict);
}

} // namespace Ksee

