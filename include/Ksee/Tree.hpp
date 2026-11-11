/**
 * @file Tree.hpp
 * @brief Hierarchical Radix Trie tree data structure supporting native types, custom typed nodes,
 *        flawless YAML/JSON parsing with comments and go-tos, and ultra-bitpacked petrification.
 */

#ifndef KSEE_TREE_HPP
#define KSEE_TREE_HPP

#include "Ksee.hpp"
#include "Array.hpp"
#include "String.hpp"
#include "Path.hpp"
#include "Func.hpp"
#include <type_traits>
#include <typeinfo>
#include <string.h>

namespace Ksee {

// -----------------------------------------------------------------------------
// Comment Class (extends String)
// -----------------------------------------------------------------------------

class Comment : public String {
public:
    using String::String;
    Comment() : String() {}
    Comment(const String &s) : String(s) {}
    Comment(String &&s) noexcept : String(std::move(s)) {}
    Comment(const char *s) : String(s) {}
    Comment& operator=(const String &s) { String::operator=(s); return *this; }
    Comment& operator=(String &&s) noexcept { String::operator=(std::move(s)); return *this; }
    Comment& operator=(const char *s) { String::operator=(s); return *this; }
    bool operator==(const char *other) const { return String::operator==(other); }
    bool operator==(const String &other) const { return String::operator==(other); }
};

// -----------------------------------------------------------------------------
// Value Type Enum
// -----------------------------------------------------------------------------

enum class TreeValueType : u8 {
    Void = 0,
    Byte = 1,       // bool
    U8 = 2,
    I32 = 3,
    U32 = 4,
    I64 = 5,
    U64 = 6,
    F32 = 7,
    F64 = 8,
    String = 9,
    Comment = 10,
    Array = 11,
    Object = 12,
    Custom = 13
};

// Standard type detection traits
template <typename T>
struct is_standard_tree : std::false_type {};

template <> struct is_standard_tree<u64> : std::true_type {};
template <> struct is_standard_tree<unsigned long> : std::true_type {};
template <> struct is_standard_tree<f64> : std::true_type {};
template <> struct is_standard_tree<u32> : std::true_type {};
template <> struct is_standard_tree<f32> : std::true_type {};
template <> struct is_standard_tree<i32> : std::true_type {};
template <> struct is_standard_tree<i64> : std::true_type {};
template <> struct is_standard_tree<long> : std::true_type {};
template <> struct is_standard_tree<u8> : std::true_type {};
template <> struct is_standard_tree<bool> : std::true_type {};
template <> struct is_standard_tree<String> : std::true_type {};
template <> struct is_standard_tree<Comment> : std::true_type {};

template <typename T>
inline constexpr bool is_standard_tree_v = is_standard_tree<std::decay_t<T>>::value;

// Custom petrify method detection
template <typename U, typename = void>
struct HasPetrify : std::false_type {};

template <typename U>
struct HasPetrify<U, std::void_t<decltype(std::declval<const U>().petrify())>> : std::true_type {};

// Custom static type name detection
template <typename U, typename = void>
struct HasStaticTypMethod : std::false_type {};

template <typename U>
struct HasStaticTypMethod<U, std::void_t<decltype(U::typ())>> : std::true_type {};

template <typename U, typename = void>
struct HasStaticTypName : std::false_type {};

template <typename U>
struct HasStaticTypName<U, std::void_t<decltype(U::typName)>> : std::true_type {};

template <typename U>
inline String getMangledCppType() {
    if constexpr (HasStaticTypMethod<U>::value) {
        return String(U::typ());
    } else if constexpr (HasStaticTypName<U>::value) {
        return String(U::typName);
    } else {
        return String(typeid(U).name());
    }
}

template <typename U>
inline String serializeVal(const U &val) {
    if constexpr (HasPetrify<U>::value) {
        return val.petrify();
    } else {
        return String(reinterpret_cast<const u8*>(&val), sizeof(U));
    }
}

// Custom static hydration / deserialization detection
template <typename U, typename = void>
struct HasStaticHydrate : std::false_type {};
template <typename U>
struct HasStaticHydrate<U, std::void_t<decltype(U::hydrate(std::declval<const String&>()))>> : std::true_type {};

template <typename U, typename = void>
struct HasStaticFromPetrified : std::false_type {};
template <typename U>
struct HasStaticFromPetrified<U, std::void_t<decltype(U::fromPetrified(std::declval<const String&>()))>> : std::true_type {};

template <typename U, typename = void>
struct HasStaticDeserialize : std::false_type {};
template <typename U>
struct HasStaticDeserialize<U, std::void_t<decltype(U::deserialize(std::declval<const String&>()))>> : std::true_type {};

template <typename U, typename = void>
struct HasStaticParse : std::false_type {};
template <typename U>
struct HasStaticParse<U, std::void_t<decltype(U::parse(std::declval<const String&>()))>> : std::true_type {};

template <typename U>
inline U instantiateFromVal(const String &val) {
    if constexpr (HasStaticHydrate<U>::value) {
        return U::hydrate(val);
    } else if constexpr (HasStaticFromPetrified<U>::value) {
        return U::fromPetrified(val);
    } else if constexpr (HasStaticDeserialize<U>::value) {
        return U::deserialize(val);
    } else if constexpr (HasStaticParse<U>::value) {
        return U::parse(val);
    } else if constexpr (std::is_constructible_v<U, const String&>) {
        return U(val);
    } else {
        U obj{};
        usz copySize = sizeof(U) < val.length() ? sizeof(U) : val.length();
        if (copySize > 0) {
            memcpy(&obj, val.data(), copySize);
        }
        return obj;
    }
}

// -----------------------------------------------------------------------------
// Tree<void> Base Class
// -----------------------------------------------------------------------------

template <typename T = void>
class Tree;

class TreeProxy;

template <>
class Tree<void> {
public:
    Path path;
    String type;
    Array<String> tags;
    String rawValue;
    Tree<void>* parent = nullptr;
    Array<Tree<void>*> children;

    Tree();
    explicit Tree(const Path &p);
    explicit Tree(const String &p);
    explicit Tree(const char *p);

    virtual ~Tree();

    Tree(const Tree<void> &other);
    Tree(Tree<void> &&other) noexcept;
    Tree<void>& operator=(const Tree<void> &other);
    Tree<void>& operator=(Tree<void> &&other) noexcept;

    virtual bool isNull() const { return children.size() == 0 && !hasValue(); }
    virtual bool hasValue() const { return !rawValue.isEmpty(); }
    virtual Tree<void>* clone() const;

    virtual int valueType() const;
    virtual String asString() const;
    virtual bool asBool() const;
    virtual f64 asNumber() const;
    virtual i64 asInt() const;
    virtual u64 asUint() const;

    template <typename ValT>
    ValT as() const;

    // Standard types handling
    bool isArray() const { return type == "Array"; }
    bool isObject() const { return type == "Object" || (type.isEmpty() && (!children.isEmpty() || !hasValue())); }
    bool isString() const { return type == "String"; }
    bool isNumber() const { return type == "f64"; }
    bool isBool() const { return type == "bool"; }
    bool isComment() const { return type == "Comment"; }

    void setArray(bool isArr = true) { type = isArr ? "Array" : "Object"; }
    void setObject(bool isObj = true) { type = isObj ? "Object" : "Array"; }

    usz size() const { return children.size(); }
    void clear();

    // Radix Tree Indexing & Query API
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

    Tree<void>* addChild(Tree<void>* c);

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

    Path generate(const Path &target = Path()) const;
    Path generate(const String &target) const { return generate(Path(target)); }
    Path generate(const char *target) const { return generate(Path(target)); }

    Tree<void>* push(const Path &target);
    Tree<void>* push(Tree<void>* child);

    template <typename ValT>
    auto push(const ValT &val);

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

    // Serialization & Parsing
    String toString() const;
    String toBinary() const;
    bool readFromBinary(const u8 *data, usz length, usz &bytesRead);

    static Tree<void> parse(const String &val);
    static Array<Tree<void>> parseMany(const String &data);

    static Tree<void>* fromString(const String &val);
    static Tree<void>* fromNumber(f64 val);
    static Tree<void>* fromBool(bool val);
};

using TreeRoot = Tree<void>;

// -----------------------------------------------------------------------------
// Typed Tree<T> Implementation
// -----------------------------------------------------------------------------

template <typename T>
class Tree : public Tree<void> {
public:
    T value;

    Tree() : Tree<void>(), value() {
        initStandardType();
    }

    explicit Tree(const T &val) : Tree<void>(), value(val) {
        initStandardType();
    }

    Tree(const Path &p, const T &val) : Tree<void>(p), value(val) {
        initStandardType();
    }

    Tree(const String &p, const T &val) : Tree<void>(p), value(val) {
        initStandardType();
    }

    Tree(const char *p, const T &val) : Tree<void>(p), value(val) {
        initStandardType();
    }

    Tree(const Tree<T> &other) : Tree<void>(other), value(other.value) {}
    Tree(Tree<T> &&other) noexcept : Tree<void>(Ksee::Move(other)), value(Ksee::Move(other.value)) {}

    Tree<T>& operator=(const Tree<T> &other) {
        if (this != &other) {
            Tree<void>::operator=(other);
            value = other.value;
        }
        return *this;
    }

    Tree<T>& operator=(Tree<T> &&other) noexcept {
        if (this != &other) {
            Tree<void>::operator=(Ksee::Move(other));
            value = Ksee::Move(other.value);
        }
        return *this;
    }

    Tree<T>& operator=(const T &val) {
        value = val;
        return *this;
    }

    bool isNull() const override { return false; }
    bool hasValue() const override { return true; }

    int valueType() const override {
        using D = std::decay_t<T>;
        if constexpr (std::is_same_v<D, bool>) return (int)TreeValueType::Byte;
        else if constexpr (std::is_same_v<D, u8>) return (int)TreeValueType::U8;
        else if constexpr (std::is_same_v<D, i32>) return (int)TreeValueType::I32;
        else if constexpr (std::is_same_v<D, u32>) return (int)TreeValueType::U32;
        else if constexpr (std::is_same_v<D, i64> || std::is_same_v<D, long>) return (int)TreeValueType::I64;
        else if constexpr (std::is_same_v<D, u64> || std::is_same_v<D, unsigned long>) return (int)TreeValueType::U64;
        else if constexpr (std::is_same_v<D, f32>) return (int)TreeValueType::F32;
        else if constexpr (std::is_same_v<D, f64>) return (int)TreeValueType::F64;
        else if constexpr (std::is_same_v<D, Comment>) return (int)TreeValueType::Comment;
        else if constexpr (std::is_same_v<D, String>) return (int)TreeValueType::String;
        else return (int)TreeValueType::Custom;
    }

    String asString() const override {
        if constexpr (std::is_same_v<T, String> || std::is_same_v<T, Comment>) {
            return value;
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_signed_v<T>) return String((long long)value);
            else return String((u64)value);
        } else if constexpr (std::is_floating_point_v<T>) {
            return String((f64)value);
        } else {
            return serializeVal<T>(value);
        }
    }

    bool asBool() const override {
        if constexpr (std::is_same_v<T, bool>) {
            return value;
        } else if constexpr (std::is_arithmetic_v<T>) {
            return value != 0;
        } else if constexpr (std::is_same_v<T, String>) {
            return value == "true" || value == "1";
        } else {
            return false;
        }
    }

    f64 asNumber() const override {
        if constexpr (std::is_arithmetic_v<T>) {
            return static_cast<f64>(value);
        } else if constexpr (std::is_same_v<T, String>) {
            return value.toDouble();
        } else {
            return 0.0;
        }
    }

    i64 asInt() const override {
        if constexpr (std::is_integral_v<T>) {
            return static_cast<i64>(value);
        } else if constexpr (std::is_floating_point_v<T>) {
            return static_cast<i64>(value);
        } else if constexpr (std::is_same_v<T, String>) {
            return value.toInt();
        } else {
            return 0;
        }
    }

    u64 asUint() const override {
        if constexpr (std::is_integral_v<T>) {
            return static_cast<u64>(value);
        } else if constexpr (std::is_floating_point_v<T>) {
            return static_cast<u64>(value);
        } else if constexpr (std::is_same_v<T, String>) {
            return static_cast<u64>(value.toInt());
        } else {
            return 0;
        }
    }

    T& operator*() { return value; }
    const T& operator*() const { return value; }

    T* operator->() { return &value; }
    const T* operator->() const { return &value; }

    Tree<void>* clone() const override {
        Tree<T>* n = new Tree<T>(path, value);
        n->type = type;
        n->tags = tags;
        for (usz i = 0; i < children.size(); ++i) {
            Tree<void>* c = children[i]->clone();
            c->parent = n;
            n->children.push(c);
        }
        return n;
    }

private:
    void initStandardType() {
        using D = std::decay_t<T>;
        if constexpr (std::is_same_v<D, bool>) {
            type = "bool";
        } else if constexpr (std::is_floating_point_v<D> || std::is_integral_v<D>) {
            type = "f64";
        } else if constexpr (std::is_same_v<D, String> || std::is_same_v<D, const char*> || std::is_same_v<D, char*>) {
            type = "String";
        } else if constexpr (std::is_same_v<D, Comment>) {
            type = "Comment";
        } else {
            type = getMangledCppType<D>();
        }
    }
};

// -----------------------------------------------------------------------------
// Template implementations for Tree<void>
// -----------------------------------------------------------------------------

template <typename ValT>
Tree<ValT>* Tree<void>::push(const Path &target, const ValT &val) {
    deleteKey(target);
    Tree<ValT>* node = new Tree<ValT>(target, val);
    node->parent = this;
    children.push(node);
    return node;
}

template <typename ValT>
auto Tree<void>::push(const ValT &val) {
    using D = std::decay_t<ValT>;
    type = "Array";
    if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*> || std::is_array_v<ValT>) {
        Tree<String>* node = new Tree<String>(String(val));
        node->parent = this;
        children.push(node);
        return node;
    } else {
        Tree<ValT>* node = new Tree<ValT>(val);
        node->parent = this;
        children.push(node);
        return node;
    }
}

template <typename ValT>
ValT Tree<void>::as() const {
    using D = std::decay_t<ValT>;
    if constexpr (std::is_same_v<D, String>) {
        return asString();
    } else if constexpr (std::is_same_v<D, Comment>) {
        return Comment(asString());
    } else if constexpr (std::is_same_v<D, bool>) {
        return asBool();
    } else if constexpr (std::is_floating_point_v<D>) {
        return static_cast<ValT>(asNumber());
    } else if constexpr (std::is_integral_v<D>) {
        if constexpr (std::is_signed_v<D>) {
            return static_cast<ValT>(asInt());
        } else {
            return static_cast<ValT>(asUint());
        }
    } else {
        if (auto* t = dynamic_cast<const Tree<ValT>*>(this)) {
            return t->value;
        }
        if (has("val")) {
            return instantiateFromVal<ValT>(get("val")->asString());
        }
        return ValT{};
    }
}

inline String Tree<void>::asString() const {
    if (auto* s = dynamic_cast<const Tree<String>*>(this)) return s->value;
    if (auto* c = dynamic_cast<const Tree<Comment>*>(this)) return c->value;
    if (auto* b = dynamic_cast<const Tree<bool>*>(this)) return b->value ? "true" : "false";
    if (auto* f = dynamic_cast<const Tree<f64>*>(this)) return String(f->value);
    if (auto* f3 = dynamic_cast<const Tree<f32>*>(this)) return String(f3->value);
    if (auto* u = dynamic_cast<const Tree<u64>*>(this)) return String(u->value);
    if (auto* u3 = dynamic_cast<const Tree<u32>*>(this)) return String((u64)u3->value);
    if (auto* u8n = dynamic_cast<const Tree<u8>*>(this)) return String((u64)u8n->value);
    if (auto* l = dynamic_cast<const Tree<i64>*>(this)) return String(l->value);
    if (auto* in = dynamic_cast<const Tree<i32>*>(this)) return String(in->value);
    if (!rawValue.isEmpty()) return rawValue;
    return String();
}

inline bool Tree<void>::asBool() const {
    if (auto* b = dynamic_cast<const Tree<bool>*>(this)) return b->value;
    if (auto* l = dynamic_cast<const Tree<i64>*>(this)) return l->value != 0;
    if (auto* in = dynamic_cast<const Tree<i32>*>(this)) return in->value != 0;
    if (auto* u = dynamic_cast<const Tree<u64>*>(this)) return u->value != 0;
    if (auto* u3 = dynamic_cast<const Tree<u32>*>(this)) return u3->value != 0;
    if (auto* u8n = dynamic_cast<const Tree<u8>*>(this)) return u8n->value != 0;
    if (auto* f = dynamic_cast<const Tree<f64>*>(this)) return f->value != 0.0;
    if (auto* s = dynamic_cast<const Tree<String>*>(this)) return s->value == "true" || s->value == "1";
    if (!rawValue.isEmpty()) return rawValue == "true" || rawValue == "1";
    return false;
}

inline f64 Tree<void>::asNumber() const {
    if (auto* f = dynamic_cast<const Tree<f64>*>(this)) return f->value;
    if (auto* f3 = dynamic_cast<const Tree<f32>*>(this)) return static_cast<f64>(f3->value);
    if (auto* l = dynamic_cast<const Tree<i64>*>(this)) return static_cast<f64>(l->value);
    if (auto* in = dynamic_cast<const Tree<i32>*>(this)) return static_cast<f64>(in->value);
    if (auto* u = dynamic_cast<const Tree<u64>*>(this)) return static_cast<f64>(u->value);
    if (auto* u3 = dynamic_cast<const Tree<u32>*>(this)) return static_cast<f64>(u3->value);
    if (auto* u8n = dynamic_cast<const Tree<u8>*>(this)) return static_cast<f64>(u8n->value);
    if (auto* s = dynamic_cast<const Tree<String>*>(this)) return s->value.toDouble();
    if (!rawValue.isEmpty()) return rawValue.toDouble();
    return 0.0;
}

inline i64 Tree<void>::asInt() const {
    if (auto* l = dynamic_cast<const Tree<i64>*>(this)) return l->value;
    if (auto* in = dynamic_cast<const Tree<i32>*>(this)) return in->value;
    if (auto* u = dynamic_cast<const Tree<u64>*>(this)) return static_cast<i64>(u->value);
    if (auto* u3 = dynamic_cast<const Tree<u32>*>(this)) return static_cast<i64>(u3->value);
    if (auto* u8n = dynamic_cast<const Tree<u8>*>(this)) return static_cast<i64>(u8n->value);
    if (auto* f = dynamic_cast<const Tree<f64>*>(this)) return static_cast<i64>(f->value);
    if (auto* s = dynamic_cast<const Tree<String>*>(this)) return s->value.toInt();
    if (!rawValue.isEmpty()) return (i64)rawValue.toLong();
    return 0;
}

inline u64 Tree<void>::asUint() const {
    if (auto* u = dynamic_cast<const Tree<u64>*>(this)) return u->value;
    if (auto* u3 = dynamic_cast<const Tree<u32>*>(this)) return u3->value;
    if (auto* u8n = dynamic_cast<const Tree<u8>*>(this)) return u8n->value;
    if (auto* l = dynamic_cast<const Tree<i64>*>(this)) return static_cast<u64>(l->value);
    if (auto* in = dynamic_cast<const Tree<i32>*>(this)) return static_cast<u64>(in->value);
    if (auto* f = dynamic_cast<const Tree<f64>*>(this)) return static_cast<u64>(f->value);
    if (auto* s = dynamic_cast<const Tree<String>*>(this)) return static_cast<u64>(s->value.toLong());
    if (!rawValue.isEmpty()) return (u64)rawValue.toLong();
    if (auto* s = dynamic_cast<const Tree<String>*>(this)) return static_cast<u64>(s->value.toInt());
    return 0;
}

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

// -----------------------------------------------------------------------------
// TreeProxy for chained assignment and access
// -----------------------------------------------------------------------------

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

    template <typename ValT, typename = typename std::enable_if<!std::is_same_v<std::decay_t<ValT>, TreeProxy> && !std::is_same_v<std::decay_t<ValT>, Tree<void>*>>::type>
    operator ValT() const {
        Tree<void>* n = node();
        return n ? n->as<ValT>() : ValT();
    }

    template <typename ValT>
    ValT as() const {
        Tree<void>* n = node();
        return n ? n->as<ValT>() : ValT();
    }

    String asString() const { return as<String>(); }
    bool asBool() const { return as<bool>(); }
    f64 asNumber() const { return as<f64>(); }
    i64 asInt() const { Tree<void>* n = node(); return n ? n->asInt() : 0; }
    u64 asUint() const { Tree<void>* n = node(); return n ? n->asUint() : 0; }
    usz size() const { Tree<void>* n = node(); return n ? n->size() : 0; }

    template <typename ValT>
    auto push(const ValT &val) {
        Tree<void>* n = node();
        if (!n) n = _parent->push(_path);
        return n->push(val);
    }

    Tree<void>* push(Tree<void>* child) {
        Tree<void>* n = node();
        if (!n) n = _parent->push(_path);
        return n->push(child);
    }

    Tree<void>* operator->() const {
        return node();
    }

    Tree<String>* operator=(const char* val) {
        if (!_parent) return nullptr;
        return _parent->push(_path, String(val));
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

    Tree<void>* at(usz idx) const {
        Tree<void>* n = node();
        return (n && idx < n->children.size()) ? n->children[idx] : nullptr;
    }

    Tree<void>* operator[](usz idx) const {
        Tree<void>* n = node();
        return (n && idx < n->children.size()) ? n->children[idx] : nullptr;
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

// -----------------------------------------------------------------------------
// Core Functional API: Stringify, Parse, Petrify, Hydrate
// -----------------------------------------------------------------------------

struct Typ {
    String name;
    Func<Tree<void>*(const String &val)> instantiate;
};
using TypHandlers = Typ;

String stringifyJSON(const Tree<void> &tree, u32 identation = 0);
String stringifyYaml(const Tree<void> &tree, u32 identation = 2);

bool parseYamlWithTyps(const Array<Typ> &th, const String &string, Tree<void> &tree);

template <typename T>
struct TypeTag { using type = T; };

template <typename... CustomTs>
inline bool parseYaml(const String &string, Tree<void> &tree) {
    Array<Typ> th;
    if constexpr (sizeof...(CustomTs) > 0) {
        auto registerTyp = [&th](auto tag) {
            using T = typename decltype(tag)::type;
            Typ t;
            t.name = getMangledCppType<T>();
            t.instantiate = [](const String &val) -> Tree<void>* {
                return new Tree<T>(instantiateFromVal<T>(val));
            };
            th.push(t);
        };
        (registerTyp(TypeTag<CustomTs>{}), ...);
    }
    return parseYamlWithTyps(th, string, tree);
}


String petrify(const Tree<void> &tree);
bool hydrate(const String &petrified, Tree<void> &tree);

} // namespace Ksee

#endif // KSEE_TREE_HPP
