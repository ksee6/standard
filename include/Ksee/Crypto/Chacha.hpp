/**
 * @file Chacha.hpp
 * @brief Standalone ChaCha cipher implementation supporting configurable rounds (default: 12).
 */

#ifndef KSEE_CRYPTO_CHACHA_HPP
#define KSEE_CRYPTO_CHACHA_HPP

#include "../Types.hpp"
#include "../String.hpp"

namespace Ksee {

static inline u32 rotl32(u32 v, int n) {
  return ((v << n) | (v >> (32 - n)));
}

inline void quarterRound(u32 &a, u32 &b, u32 &c, u32 &d) {
  a += b; d ^= a; d = rotl32(d, 16);
  c += d; b ^= c; b = rotl32(b, 12);
  a += b; d ^= a; d = rotl32(d, 8);
  c += d; b ^= c; b = rotl32(b, 7);
}

String createIetfNonce(u64 nonce);

u512 chacha(u256 key, u64 nonce, u8 rounds = 12);
u512 chacha(u256 key, u64 nonce, u8 rounds, u32 counter);

String crypt(const String &input, u256 key, u64 nonce, u8 rounds = 12);
String crypt(const char *input, usz length, u256 key, u64 nonce, u8 rounds = 12);

void chacha(u8 *out, const u8 *in, usz len, const u8 key[32], const u8 nonce[12], u32 counter = 1, int rounds = 12);

inline String chacha(const String &input, const String &key, u64 nonce, int rounds = 12) {
  return crypt(input, u256(key), nonce, (u8)rounds);
}

inline String chacha(const char *input, usz length, const String &key, u64 nonce, int rounds = 12) {
  return crypt(input, length, u256(key), nonce, (u8)rounds);
}

} // namespace Ksee

#endif // KSEE_CRYPTO_CHACHA_HPP

