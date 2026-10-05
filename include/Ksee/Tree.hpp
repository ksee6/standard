/**
 * @file Tree.hpp
 * @brief Radix Trie tree data structure supporting Path indexing, pattern matching, and typed values.
 */

#ifndef KSEE_TREE_HPP
#define KSEE_TREE_HPP

#include "Types.hpp"
#include "Array.hpp"
#include "String.hpp"
#include "Path.hpp"
#include <type_traits>

namespace Ksee {

template <typename T>
class Tree;

class TreeProxy;

template <>
class Tree<void> {
public:
    Array<Tree<void>*> children;
    Tree<void>* parent = nullptr;
    Path path;

    Tree();
    explicit Tree(const Path &p);
    explicit Tree(const String &p);
    explicit Tree(const char *p);

    virtual ~Tree();

    Tree(const Tree<void> &other);
    Tree(Tree<void> &&other) noexcept;
    Tree<void>& operator=(const Tree<void> &other);
    Tree<void>& operator=(Tree<void> &&other) noexcept;

    virtual bool isNull() const { return children.size() == 0; }
    virtual bool hasValue() const { return false; }
    virtual Tree<void>* clone() const;

    virtual int valueType() const { return 0; }
    virtual String asString() const;
    virtual bool asBool() const;
    virtual f64 asNumber() const;

    static Tree<void>* fromString(const String &val);
    static Tree<void>* fromNumber(f64 val);
    static Tree<void>* fromBool(bool val);

    template <typename ValT>
    ValT as() const;

    usz size() const { return children.size(); }
    void clear();

    // Radix Tree API
    Array<Tree<void>*> queryAll(const Path &target, bool strict = true) const;
    Array<Tree<void>*> queryAll(const String &target, bool strict = true) const;
    Array<Tree<void>*> queryAll(const char *target, bool strict = true) const;

    Tree<void>* query(const Path &target, bool strict = true) const;
    Tree<void>* query(const String &target, bool strict = true) const;
    Tree<void>* query(const char *target, bool strict = true) const;

    Tree<void>* get(const Path &target) const;
    Tree<void>* get(const String &target) const;
    Tree<void>* get(const char *target) const;

    Tree<void>* first(const Path &target) const { return get(target); }
    Tree<void>* first(const String &target) const { return get(target); }
    Tree<void>* first(const char *target) const { return get(target); }

    Tree<void>* addChild(Tree<void>* c) {
        if (c) {
            c->parent = this;
            children.push(c);
        }
        return c;
    }

    template <typename ValT>
    Tree<ValT>* addChild(Tree<ValT>* c) {
        if (c) {
            c->parent = this;
            children.push(c);
        }
        return c;
    }

    bool has(const Path &target) const;
    bool has(const String &target) const;
    bool has(const char *target) const;

    bool deleteKey(const Path &target);
    bool deleteKey(const String &target);
    bool deleteKey(const char *target);

    Tree<void>* push(const Path &target);
    Tree<void>* push(const String &target);
    Tree<void>* push(const char *target);

    template <typename ValT>
    Tree<ValT>* push(const Path &target, const ValT &val);

    template <typename ValT>
    Tree<ValT>* push(const String &target, const ValT &val) {
        return push(Path(target), val);
    }

    template <typename ValT>
    Tree<ValT>* push(const char *target, const ValT &val) {
        return push(Path(target), val);
    }

    // Direct indexed access
    TreeProxy operator[](const Path &target);
    TreeProxy operator[](const String &target);
    TreeProxy operator[](const char *target);
    Tree<void>* operator[](usz idx) const {
        return (idx < children.size()) ? children[idx] : nullptr;
    }
};

template <typename T>
class Tree : public Tree<void> {
public:
    T value;

    Tree() : Tree<void>(), value() {}
    explicit Tree(const T &val) : Tree<void>(), value(val) {}
    Tree(const Path &p, const T &val) : Tree<void>(p), value(val) {}
    Tree(const String &p, const T &val) : Tree<void>(p), value(val) {}
    Tree(const char *p, const T &val) : Tree<void>(p), value(val) {}

    bool isNull() const override { return false; }
    bool hasValue() const override { return true; }

    int valueType() const override {
        if constexpr (std::is_same_v<T, String>) return 3;
        if constexpr (std::is_same_v<T, bool>) return 4;
        if constexpr (std::is_arithmetic_v<T>) return 2;
        return 1;
    }

    T& operator*() { return value; }
    const T& operator*() const { return value; }

    T* operator->() { return &value; }
    const T* operator->() const { return &value; }

    Tree<void>* clone() const override {
        Tree<T>* n = new Tree<T>(path, value);
        for (usz i = 0; i < children.size(); ++i) {
            Tree<void>* c = children[i]->clone();
            c->parent = n;
            n->children.push(c);
        }
        return n;
    }
};

template <typename ValT>
Tree<ValT>* Tree<void>::push(const Path &target, const ValT &val) {
    deleteKey(target);
    Tree<ValT>* node = new Tree<ValT>(target, val);
    node->parent = this;
    children.push(node);
    return node;
}

template <typename ValT>
ValT Tree<void>::as() const {
    if constexpr (std::is_same_v<ValT, String>) {
        if (auto* s = dynamic_cast<const Tree<String>*>(this)) return s->value;
        if (auto* b = dynamic_cast<const Tree<bool>*>(this)) return b->value ? "true" : "false";
        if (auto* f = dynamic_cast<const Tree<f64>*>(this)) return String(f->value);
        if (auto* l = dynamic_cast<const Tree<long long>*>(this)) return String(l->value);
        return String();
    } else if constexpr (std::is_same_v<ValT, bool>) {
        if (auto* b = dynamic_cast<const Tree<bool>*>(this)) return b->value;
        if (auto* l = dynamic_cast<const Tree<long long>*>(this)) return l->value != 0;
        if (auto* f = dynamic_cast<const Tree<f64>*>(this)) return f->value != 0.0;
        if (auto* s = dynamic_cast<const Tree<String>*>(this)) return s->value == "true" || s->value == "1";
        return false;
    } else if constexpr (std::is_floating_point_v<ValT>) {
        if (auto* f = dynamic_cast<const Tree<f64>*>(this)) return static_cast<ValT>(f->value);
        if (auto* f3 = dynamic_cast<const Tree<f32>*>(this)) return static_cast<ValT>(f3->value);
        if (auto* l = dynamic_cast<const Tree<long long>*>(this)) return static_cast<ValT>(l->value);
        if (auto* i = dynamic_cast<const Tree<int>*>(this)) return static_cast<ValT>(i->value);
        if (auto* u = dynamic_cast<const Tree<u64>*>(this)) return static_cast<ValT>(u->value);
        if (auto* u3 = dynamic_cast<const Tree<u32>*>(this)) return static_cast<ValT>(u3->value);
        if (auto* s = dynamic_cast<const Tree<String>*>(this)) return static_cast<ValT>(s->value.toDouble());
        return static_cast<ValT>(0);
    } else if constexpr (std::is_integral_v<ValT>) {
        if (auto* l = dynamic_cast<const Tree<long long>*>(this)) return static_cast<ValT>(l->value);
        if (auto* i = dynamic_cast<const Tree<int>*>(this)) return static_cast<ValT>(i->value);
        if (auto* u = dynamic_cast<const Tree<u64>*>(this)) return static_cast<ValT>(u->value);
        if (auto* u3 = dynamic_cast<const Tree<u32>*>(this)) return static_cast<ValT>(u3->value);
        if (auto* f = dynamic_cast<const Tree<f64>*>(this)) return static_cast<ValT>(f->value);
        if (auto* s = dynamic_cast<const Tree<String>*>(this)) return static_cast<ValT>(s->value.toLong());
        return static_cast<ValT>(0);
    } else {
        if (auto* t = dynamic_cast<const Tree<ValT>*>(this)) return t->value;
        return ValT();
}
}

inline String Tree<void>::asString() const { return as<String>(); }
inline bool Tree<void>::asBool() const { return as<bool>(); }
inline f64 Tree<void>::asNumber() const { return as<f64>(); }

inline Tree<void>* Tree<void>::fromString(const String &val) { return new Tree<String>(val); }
inline Tree<void>* Tree<void>::fromNumber(f64 val) { return new Tree<f64>(val); }
inline Tree<void>* Tree<void>::fromBool(bool val) { return new Tree<bool>(val); }

// Deduction guides for C++17
Tree() -> Tree<void>;
Tree(const Path &) -> Tree<void>;
Tree(const String &) -> Tree<void>;
Tree(const char *) -> Tree<void>;

template <typename T>
Tree(const T &) -> Tree<T>;

template <typename T>
Tree(const Path &, const T &) -> Tree<T>;

template <typename T>
Tree(const String &, const T &) -> Tree<T>;

template <typename T>
Tree(const char *, const T &) -> Tree<T>;

class TreeProxy {
private:
    Tree<void>* _parent;
    Path _path;

public:
    TreeProxy(Tree<void>* parent, const Path &path) : _parent(parent), _path(path) {}

    operator Tree<void>*() const {
        return _parent ? _parent->get(_path) : nullptr;
    }

    Tree<void>* node() const {
        return _parent ? _parent->get(_path) : nullptr;
    }

    Tree<void>* operator->() const {
        return node();
    }

    template <typename ValT>
    Tree<ValT>* operator=(const ValT &val) {
        if (!_parent) return nullptr;
        return _parent->push(_path, val);
    }

    Tree<void>* operator=(Tree<void>* otherNode) {
        if (!_parent) return nullptr;
        _parent->deleteKey(_path);
        if (otherNode) {
            Tree<void>* c = otherNode->clone();
            c->path = _path;
            c->parent = _parent;
            _parent->children.push(c);
            return c;
        }
        return nullptr;
    }

    TreeProxy operator[](const Path &target) {
        Tree<void>* n = node();
        if (!n) {
            n = _parent->push(_path);
        }
        return TreeProxy(n, target);
    }

    TreeProxy operator[](const String &target) {
        return operator[](Path(target));
    }

    TreeProxy operator[](const char *target) {
        return operator[](Path(target));
    }
};

inline TreeProxy Tree<void>::operator[](const Path &target) {
    return TreeProxy(this, target);
}

inline TreeProxy Tree<void>::operator[](const String &target) {
    return TreeProxy(this, Path(target));
}

inline TreeProxy Tree<void>::operator[](const char *target) {
    return TreeProxy(this, Path(target));
}

} // namespace Ksee

#endif // KSEE_TREE_HPP

