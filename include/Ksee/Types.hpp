/**
 * @file Types.hpp
 * @brief Core primitives, type traits, allocators, basic random, and uptime clock for Ksee.
 */

#ifndef KSEE_TYPES_HPP
#define KSEE_TYPES_HPP

#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
#include <time.h>
#endif

#ifdef __KERNEL__
#include <linux/delay.h>
#endif



#if !defined(__KERNEL__) && !defined(KSEE_NO_STD)
#if defined(__has_include)
#if __has_include(<new>)
#include <new>
#define __PLACEMENT_NEW_INLINE
#endif
#elif defined(__cplusplus) && __cplusplus >= 201103L
#include <new>
#define __PLACEMENT_NEW_INLINE
#endif
#endif

namespace Ksee {

class String;

// -------------------------------------------------------------------------
// Integer & Float Primitives
// -------------------------------------------------------------------------

using usz = decltype(sizeof(0));

using u8 = unsigned char;
using i8 = signed char;

#if __SIZEOF_INT__ == 2
using u16 = unsigned int;
using i16 = int;
using u32 = unsigned long;
using i32 = long;
#else
using u16 = unsigned short;
using i16 = short;
using u32 = unsigned int;
using i32 = int;
#endif

using u64 = unsigned long long;
using i64 = long long;

// -------------------------------------------------------------------------
// Fixed-width Byte Array Types (128-bit, 256-bit, 512-bit)
// -------------------------------------------------------------------------

struct u128 {
  u8 data[16];

  constexpr u128() : data{} {}
  constexpr u128(const u8 *ptr) : data{} {
    if (ptr) {
      for (usz i = 0; i < 16; ++i) data[i] = ptr[i];
    }
  }
  constexpr u128(u64 hi, u64 lo) : data{} {
    for (int i = 0; i < 8; ++i) {
      data[7 - i] = (u8)((hi >> (i * 8)) & 0xFF);
      data[15 - i] = (u8)((lo >> (i * 8)) & 0xFF);
    }
  }
  constexpr u128(u64 val) : u128(0, val) {}

  u128(const String &str);
  String asString() const;
  String toHex() const;

  u8 &operator[](usz i) { return data[i]; }
  const u8 &operator[](usz i) const { return data[i]; }

  u8 *begin() { return data; }
  const u8 *begin() const { return data; }
  u8 *end() { return data + 16; }
  const u8 *end() const { return data + 16; }

  constexpr usz size() const { return 16; }

  bool operator==(const u128 &o) const {
    for (usz i = 0; i < 16; ++i) if (data[i] != o.data[i]) return false;
    return true;
  }
  bool operator!=(const u128 &o) const { return !(*this == o); }
  bool operator<(const u128 &o) const {
    for (usz i = 0; i < 16; ++i) {
      if (data[i] < o.data[i]) return true;
      if (data[i] > o.data[i]) return false;
    }
    return false;
  }
  bool operator>(const u128 &o) const { return o < *this; }
  bool operator<=(const u128 &o) const { return !(o < *this); }
  bool operator>=(const u128 &o) const { return !(*this < o); }

  bool isNil() const {
    for (usz i = 0; i < 16; ++i) if (data[i] != 0) return false;
    return true;
  }
  explicit operator bool() const { return !isNil(); }
};

struct u256 {
  u8 data[32];

  constexpr u256() : data{} {}
  constexpr u256(const u8 *ptr) : data{} {
    if (ptr) {
      for (usz i = 0; i < 32; ++i) data[i] = ptr[i];
    }
  }
  constexpr u256(u64 a, u64 b, u64 c, u64 d) : data{} {
    u64 parts[4] = {a, b, c, d};
    for (int p = 0; p < 4; ++p) {
      for (int i = 0; i < 8; ++i) {
        data[p * 8 + (7 - i)] = (u8)((parts[p] >> (i * 8)) & 0xFF);
      }
    }
  }

  u256(const String &str);
  String asString() const;
  String toHex() const;

  u8 &operator[](usz i) { return data[i]; }
  const u8 &operator[](usz i) const { return data[i]; }

  u8 *begin() { return data; }
  const u8 *begin() const { return data; }
  u8 *end() { return data + 32; }
  const u8 *end() const { return data + 32; }

  constexpr usz size() const { return 32; }

  bool operator==(const u256 &o) const {
    for (usz i = 0; i < 32; ++i) if (data[i] != o.data[i]) return false;
    return true;
  }
  bool operator!=(const u256 &o) const { return !(*this == o); }
  bool operator<(const u256 &o) const {
    for (usz i = 0; i < 32; ++i) {
      if (data[i] < o.data[i]) return true;
      if (data[i] > o.data[i]) return false;
    }
    return false;
  }
  bool operator>(const u256 &o) const { return o < *this; }
  bool operator<=(const u256 &o) const { return !(o < *this); }
  bool operator>=(const u256 &o) const { return !(*this < o); }

  bool isNil() const {
    for (usz i = 0; i < 32; ++i) if (data[i] != 0) return false;
    return true;
  }
  explicit operator bool() const { return !isNil(); }
};

struct u512 {
  u8 data[64];

  constexpr u512() : data{} {}
  constexpr u512(const u8 *ptr) : data{} {
    if (ptr) {
      for (usz i = 0; i < 64; ++i) data[i] = ptr[i];
    }
  }

  u512(const String &str);
  String asString() const;
  String toHex() const;

  u8 &operator[](usz i) { return data[i]; }
  const u8 &operator[](usz i) const { return data[i]; }

  u8 *begin() { return data; }
  const u8 *begin() const { return data; }
  u8 *end() { return data + 64; }
  const u8 *end() const { return data + 64; }

  constexpr usz size() const { return 64; }

  bool operator==(const u512 &o) const {
    for (usz i = 0; i < 64; ++i) if (data[i] != o.data[i]) return false;
    return true;
  }
  bool operator!=(const u512 &o) const { return !(*this == o); }
  bool operator<(const u512 &o) const {
    for (usz i = 0; i < 64; ++i) {
      if (data[i] < o.data[i]) return true;
      if (data[i] > o.data[i]) return false;
    }
    return false;
  }
  bool operator>(const u512 &o) const { return o < *this; }
  bool operator<=(const u512 &o) const { return !(o < *this); }
  bool operator>=(const u512 &o) const { return !(*this < o); }

  bool isNil() const {
    for (usz i = 0; i < 64; ++i) if (data[i] != 0) return false;
    return true;
  }
  explicit operator bool() const { return !isNil(); }
};

using f32 = float;
using f64 = double;

static constexpr decltype(nullptr) null = nullptr;

// -------------------------------------------------------------------------
// Metaprogramming Utilities
// -------------------------------------------------------------------------

template <typename T> struct RemoveRef { using Type = T; };
template <typename T> struct RemoveRef<T &> { using Type = T; };
template <typename T> struct RemoveRef<T &&> { using Type = T; };

template <typename T>
inline typename RemoveRef<T>::Type &&Move(T &&arg) noexcept {
  return static_cast<typename RemoveRef<T>::Type &&>(arg);
}

template <bool B, typename T = void> struct EnableIf {};
template <typename T> struct EnableIf<true, T> { using Type = T; };

template <typename T> inline void Swap(T &a, T &b) {
  T temp = Ksee::Move(a);
  a = Ksee::Move(b);
  b = Ksee::Move(temp);
}

template <typename T> T &&DeclVal() noexcept;

template <typename U, typename V> struct IsSame { static const bool Value = false; };
template <typename U> struct IsSame<U, U> { static const bool Value = true; };

template <typename T> struct RemoveConst { using Type = T; };
template <typename T> struct RemoveConst<const T> { using Type = T; };

template <typename T> struct Decay {
  using Type = typename RemoveConst<typename RemoveRef<T>::Type>::Type;
};

template <typename T> struct IsIntegralHelper { static const bool Value = false; };
template <> struct IsIntegralHelper<bool> { static const bool Value = true; };
template <> struct IsIntegralHelper<char> { static const bool Value = true; };
template <> struct IsIntegralHelper<signed char> { static const bool Value = true; };
template <> struct IsIntegralHelper<unsigned char> { static const bool Value = true; };
template <> struct IsIntegralHelper<short> { static const bool Value = true; };
template <> struct IsIntegralHelper<unsigned short> { static const bool Value = true; };
template <> struct IsIntegralHelper<int> { static const bool Value = true; };
template <> struct IsIntegralHelper<unsigned int> { static const bool Value = true; };
template <> struct IsIntegralHelper<long> { static const bool Value = true; };
template <> struct IsIntegralHelper<unsigned long> { static const bool Value = true; };
template <> struct IsIntegralHelper<long long> { static const bool Value = true; };
template <> struct IsIntegralHelper<unsigned long long> { static const bool Value = true; };

template <typename T> struct IsIntegral {
  static const bool Value = IsIntegralHelper<typename Decay<T>::Type>::Value;
};

template <typename... Ts> struct MaxSize;
template <typename T> struct MaxSize<T> { static constexpr usz Value = sizeof(T); };
template <typename T, typename... Ts> struct MaxSize<T, Ts...> {
  static constexpr usz rest = MaxSize<Ts...>::Value;
  static constexpr usz Value = sizeof(T) > rest ? sizeof(T) : rest;
};

template <typename... Ts> struct MaxAlign;
template <typename T> struct MaxAlign<T> { static constexpr usz Value = alignof(T); };
template <typename T, typename... Ts> struct MaxAlign<T, Ts...> {
  static constexpr usz rest = MaxAlign<Ts...>::Value;
  static constexpr usz Value = alignof(T) > rest ? alignof(T) : rest;
};

template <typename T, typename... Ts> struct TypeIndex;
template <typename T, typename... Ts> struct TypeIndex<T, T, Ts...> {
  static constexpr u32 Value = 0;
};
template <typename T, typename U, typename... Ts> struct TypeIndex<T, U, Ts...> {
  static constexpr u32 Value = 1 + TypeIndex<T, Ts...>::Value;
};

template <typename... Ts>
class Either {
  alignas(MaxAlign<Ts...>::Value) u8 data[MaxSize<Ts...>::Value];
  i32 type_id = -1;
  void (*destroy_fn)(void*) = nullptr;
  void (*copy_fn)(void*, const void*) = nullptr;
  void (*move_fn)(void*, void*) = nullptr;

public:
  Either() {}

  template <typename T, typename = typename EnableIf<!IsSame<typename Decay<T>::Type, Either>::Value>::Type>
  Either(T&& val) {
    using Decayed = typename Decay<T>::Type;
    type_id = (i32)TypeIndex<Decayed, Ts...>::Value;
    new (data) Decayed(Ksee::Move(val));
    destroy_fn = [](void* ptr) { static_cast<Decayed*>(ptr)->~Decayed(); };
    copy_fn = [](void* dst, const void* src) { new (dst) Decayed(*static_cast<const Decayed*>(src)); };
    move_fn = [](void* dst, void* src) { new (dst) Decayed(Ksee::Move(*static_cast<Decayed*>(src))); };
  }

  Either(const Either& other) : type_id(other.type_id), destroy_fn(other.destroy_fn), copy_fn(other.copy_fn), move_fn(other.move_fn) {
    if (copy_fn) copy_fn(data, other.data);
  }

  Either(Either&& other) : type_id(other.type_id), destroy_fn(other.destroy_fn), copy_fn(other.copy_fn), move_fn(other.move_fn) {
    if (move_fn) move_fn(data, other.data);
    other.type_id = -1;
    other.destroy_fn = nullptr;
    other.copy_fn = nullptr;
    other.move_fn = nullptr;
  }

  Either& operator=(const Either& other) {
    if (this != &other) {
      if (destroy_fn) destroy_fn(data);
      type_id = other.type_id;
      destroy_fn = other.destroy_fn;
      copy_fn = other.copy_fn;
      move_fn = other.move_fn;
      if (copy_fn) copy_fn(data, other.data);
    }
    return *this;
  }

  Either& operator=(Either&& other) {
    if (this != &other) {
      if (destroy_fn) destroy_fn(data);
      type_id = other.type_id;
      destroy_fn = other.destroy_fn;
      copy_fn = other.copy_fn;
      move_fn = other.move_fn;
      if (move_fn) move_fn(data, other.data);
      other.type_id = -1;
      other.destroy_fn = nullptr;
      other.copy_fn = nullptr;
      other.move_fn = nullptr;
    }
    return *this;
  }

  ~Either() {
    if (destroy_fn) destroy_fn(data);
  }

  template <typename T>
  bool is() const {
    return type_id == (i32)TypeIndex<T, Ts...>::Value;
  }

  template <typename T>
  T& get() {
    return *reinterpret_cast<T*>(data);
  }

  template <typename T>
  const T& get() const {
    return *reinterpret_cast<const T*>(data);
  }
};

template <typename T> struct Equal {
  static bool eq(const T &a, const T &b) { return a == b; }
};

template <> struct Equal<const char *> {
  static bool eq(const char *a, const char *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    while (*a && *b) {
      if (*a != *b) return false;
      a++;
      b++;
    }
    return *a == *b;
  }
};

#ifndef PI
static constexpr f64 PI = 3.14159265358979323846;
#endif
static constexpr f64 E = 2.71828182845904523536;

template <typename T> struct FNVHasher {
  static usz fnvHash(const T &key) {
    const char *ptr = (const char *)&key;
#if __SIZEOF_POINTER__ == 8
    usz fnvHash = 14695981039346656037ULL;
    const usz prime = 1099511628211ULL;
#else
    usz fnvHash = 2166136261U;
    const usz prime = 16777619U;
#endif
    for (usz i = 0; i < sizeof(T); ++i) {
      fnvHash ^= (usz)((u8)ptr[i]);
      fnvHash *= prime;
    }
    return fnvHash;
  }
};

template <typename T> struct FNVHasher<T *> {
  static usz fnvHash(T *key) {
    usz k = (usz)key;
#if __SIZEOF_POINTER__ == 8
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
#else
    k ^= k >> 16;
    k *= 0x85ebca6b;
    k ^= k >> 13;
    k *= 0xc2b2ae35;
    k ^= k >> 16;
#endif
    return k;
  }
};

usz fnvHashMix(usz k);

// -------------------------------------------------------------------------
// Allocators / Memory Device Interface
// -------------------------------------------------------------------------

class IMemoryDevice {
public:
  virtual void *alloc(usz size) = 0;
  virtual void free(void *handle) = 0;
  virtual void upload(void *handle, const void *src, usz size) = 0;
  virtual void download(void *handle, void *dst, usz size) = 0;
  virtual void *view(void *handle, i32 type = 0) = 0;
  virtual void *allocSurface(i32 w, i32 h, i32 channels = 4) = 0;
  virtual ~IMemoryDevice() = default;
};

class MemoryDevice : public IMemoryDevice {
public:
  virtual void *alloc(usz size) override = 0;
  virtual void free(void *handle) override = 0;
  virtual void upload(void *handle, const void *src, usz size) override = 0;
  virtual void download(void *handle, void *dst, usz size) override = 0;

  virtual void *view(void *handle, i32 type = 0) override {
    (void)type;
    return handle;
  }

  virtual void *allocSurface(i32 w, i32 h, i32 channels = 4) override {
    return alloc((usz)(w * h * channels));
  }

  virtual ~MemoryDevice() = default;
};

// -------------------------------------------------------------------------
// Basic Random (u32 and buffer filling)
// -------------------------------------------------------------------------

void randomSeed(u64 seed, bool overwrite = false);
void randomSeed(bool overwrite = false);
u32 random();
u32 random(u32 max);
i32 random(i32 min, i32 max);
void randomFill(u8 *buffer, usz size);

// -------------------------------------------------------------------------
// Uptime & Epoch Clock
// -------------------------------------------------------------------------

extern bool isEpochTimeSet;
extern u64 epochOnStart;

bool autoSetEpochTime();
u64 millis();
u64 nanos();
u64 micros();
u64 epochMillis();

// -------------------------------------------------------------------------
// Crockford's Base32
// -------------------------------------------------------------------------

class String;

String encodeCB32BigEndian(const u8 *ptr, usz length);
usz decodeCB32BigEndian(const String &str, u8 *out, usz maxLength);

String encodeCB32(u32 v);
String encodeCB32(u64 v);
String encodeCB32(const u128 &v);
String encodeCB32(const u256 &v);
String encodeCB32(const u512 &v);

struct CB32Decoder {
  const String &str;
  operator u32() const;
  operator u64() const;
  operator u128() const;
  operator u256() const;
  operator u512() const;
  u32 toU32() const;
  u64 toU64() const;
  u128 toU128() const;
  u256 toU256() const;
  u512 toU512() const;
};

CB32Decoder decodeCB32(const String &str);
bool decodeCB32(const String &str, u32 &out);
bool decodeCB32(const String &str, u64 &out);
bool decodeCB32(const String &str, u128 &out);
bool decodeCB32(const String &str, u256 &out);
bool decodeCB32(const String &str, u512 &out);

class Regex;

template <typename T> struct HasSerialize {
private:
  template <typename U>
  static auto test(int) -> decltype(Ksee::DeclVal<U>().serialize(), char());
  template <typename U> static long test(...);
public:
  static const bool Value = sizeof(test<T>(0)) == sizeof(char);
};

template <typename T> struct HasDeserialize {
private:
  template <typename U>
  static auto test(int) -> decltype(U::deserialize(Ksee::DeclVal<Ksee::String>()), char());
  template <typename U> static long test(...);
public:
  static const bool Value = sizeof(test<T>(0)) == sizeof(char);
};

template <typename T> struct HasDeserializeAt {
private:
  template <typename U>
  static auto test(int) -> decltype(U::deserialize(Ksee::DeclVal<Ksee::String>(), Ksee::DeclVal<usz &>()), char());
  template <typename U> static long test(...);
public:
  static const bool Value = sizeof(test<T>(0)) == sizeof(char);
};

template <typename T> struct IsPrimitive {
  static const bool Value = false;
};

#define KSEE_MARK_PRIMITIVE(Ty) \
  template <> struct IsPrimitive<Ty> { static const bool Value = true; };

KSEE_MARK_PRIMITIVE(bool)
KSEE_MARK_PRIMITIVE(u8)
KSEE_MARK_PRIMITIVE(i8)
KSEE_MARK_PRIMITIVE(u16)
KSEE_MARK_PRIMITIVE(i16)
KSEE_MARK_PRIMITIVE(u32)
KSEE_MARK_PRIMITIVE(i32)
KSEE_MARK_PRIMITIVE(u64)
KSEE_MARK_PRIMITIVE(i64)
KSEE_MARK_PRIMITIVE(f32)
KSEE_MARK_PRIMITIVE(f64)

#undef KSEE_MARK_PRIMITIVE

template <typename T, typename S = String>
S serialize(const T &obj);

template <typename T, typename S = String>
T deserialize(const S &s, usz &at);

} // namespace Ksee

#endif // KSEE_TYPES_HPP
