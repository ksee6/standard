/**
 * @file Base32.hpp
 * @brief Base32 (RFC 4648) and Crockford's Base32 encoding and decoding.
 */

#ifndef KSEE_BASE32_HPP
#define KSEE_BASE32_HPP

#include "Ksee.hpp"
#include "String.hpp"

namespace Ksee {

// -------------------------------------------------------------------------
// Crockford's Base32 (CB32)
// -------------------------------------------------------------------------

String encodeCB32BigEndian(const u8 *ptr, usz length);
usz decodeCB32BigEndian(const String &str, u8 *out, usz maxLength);

String encodeCB32(u32 v);
String encodeCB32(u64 v);
String encodeCB32(const u128 &v);
String encodeCB32(const u256 &v);
String encodeCB32(const u512 &v);
String encodeCB32(const String &str);

struct CB32Decoder {
  const String &str;
  operator u32() const;
  operator u64() const;
  operator u128() const;
  operator u256() const;
  operator u512() const;
  operator String() const;
  u32 toU32() const;
  u64 toU64() const;
  u128 toU128() const;
  u256 toU256() const;
  u512 toU512() const;
  String toString() const;
};

CB32Decoder decodeCB32(const String &str);
bool decodeCB32(const String &str, u32 &out);
bool decodeCB32(const String &str, u64 &out);
bool decodeCB32(const String &str, u128 &out);
bool decodeCB32(const String &str, u256 &out);
bool decodeCB32(const String &str, u512 &out);
bool decodeCB32(const String &str, String &out);

// -------------------------------------------------------------------------
// Standard RFC 4648 Base32 (B32)
// -------------------------------------------------------------------------

String encodeB32BigEndian(const u8 *ptr, usz length);
usz decodeB32BigEndian(const String &str, u8 *out, usz maxLength);

String encodeB32(u32 v);
String encodeB32(u64 v);
String encodeB32(const u128 &v);
String encodeB32(const u256 &v);
String encodeB32(const u512 &v);
String encodeB32(const String &str);

struct B32Decoder {
  const String &str;
  operator u32() const;
  operator u64() const;
  operator u128() const;
  operator u256() const;
  operator u512() const;
  operator String() const;
  u32 toU32() const;
  u64 toU64() const;
  u128 toU128() const;
  u256 toU256() const;
  u512 toU512() const;
  String toString() const;
};

B32Decoder decodeB32(const String &str);
bool decodeB32(const String &str, u32 &out);
bool decodeB32(const String &str, u64 &out);
bool decodeB32(const String &str, u128 &out);
bool decodeB32(const String &str, u256 &out);
bool decodeB32(const String &str, u512 &out);
bool decodeB32(const String &str, String &out);

} // namespace Ksee

#endif // KSEE_BASE32_HPP
