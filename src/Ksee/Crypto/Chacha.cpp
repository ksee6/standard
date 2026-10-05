/**
 * @file Chacha.cpp
 * @brief Standalone ChaCha cipher implementation supporting configurable rounds (default: 12).
 */

#include "../../../include/Ksee/Crypto/Chacha.hpp"

namespace Ksee {

static inline u32 load32_le(const u8 *p) {
  return ((u32)p[0]) | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static inline void store32_le(u8 *p, u32 v) {
  p[0] = (u8)v;
  p[1] = (u8)(v >> 8);
  p[2] = (u8)(v >> 16);
  p[3] = (u8)(v >> 24);
}

String createIetfNonce(u64 nonce) {
  String buffer;
  u8 tmp[12];
  for (int i = 0; i < 4; ++i) tmp[i] = 0;
  for (int i = 0; i < 8; ++i) tmp[4 + i] = (u8)((nonce >> (i * 8)) & 0xFF);
  buffer.pushEach(tmp, 12);
  return buffer;
}

u512 chacha(u256 key, u64 nonce, u8 rounds) {
  return chacha(key, nonce, rounds, 1);
}

u512 chacha(u256 key, u64 nonce, u8 rounds, u32 counter) {
  String n = createIetfNonce(nonce);
  u32 ctx[16];
  ctx[0] = 0x61707865;
  ctx[1] = 0x3320646e;
  ctx[2] = 0x79622d32;
  ctx[3] = 0x6b206574;
  for (int i = 0; i < 8; ++i) ctx[4 + i] = load32_le(key.data + i * 4);
  ctx[12] = counter;
  for (int i = 0; i < 3; ++i) ctx[13 + i] = load32_le((const u8*)n.data() + i * 4);

  u32 x[16];
  for (int i = 0; i < 16; ++i) x[i] = ctx[i];
  int doubleRounds = rounds / 2;
  if (doubleRounds < 1) doubleRounds = 6;
  for (int r = 0; r < doubleRounds; ++r) {
    quarterRound(x[0], x[4], x[8],  x[12]);
    quarterRound(x[1], x[5], x[9],  x[13]);
    quarterRound(x[2], x[6], x[10], x[14]);
    quarterRound(x[3], x[7], x[11], x[15]);
    quarterRound(x[0], x[5], x[10], x[15]);
    quarterRound(x[1], x[6], x[11], x[12]);
    quarterRound(x[2], x[7], x[8],  x[13]);
    quarterRound(x[3], x[4], x[9],  x[14]);
  }
  u512 out;
  for (int i = 0; i < 16; ++i) store32_le(out.data + i * 4, x[i] + ctx[i]);
  return out;
}

String crypt(const char *input, usz length, u256 key, u64 nonce, u8 rounds) {
  if (length == 0) return String();
  String n = createIetfNonce(nonce);
  String result;
  u8 *buf = new u8[length];
  chacha(buf, (const u8*)input, length, key.data, (const u8*)n.data(), 1, rounds);
  result.pushEach(buf, length);
  delete[] buf;
  return result;
}

String crypt(const String &input, u256 key, u64 nonce, u8 rounds) {
  return crypt(input.c_str(), input.size(), key, nonce, rounds);
}

void chacha(u8 *out, const u8 *in, usz len, const u8 key[32], const u8 nonce[12], u32 counter, int rounds) {
  u32 ctx[16];
  ctx[0] = 0x61707865;
  ctx[1] = 0x3320646e;
  ctx[2] = 0x79622d32;
  ctx[3] = 0x6b206574;
  for (int i = 0; i < 8; ++i) ctx[4 + i] = load32_le(key + i * 4);
  ctx[12] = counter;
  for (int i = 0; i < 3; ++i) ctx[13 + i] = load32_le(nonce + i * 4);

  u8 block[64];
  int doubleRounds = rounds / 2;
  if (doubleRounds < 1) doubleRounds = 6;

  while (len > 0) {
    u32 x[16];
    for (int i = 0; i < 16; ++i) x[i] = ctx[i];
    for (int r = 0; r < doubleRounds; ++r) {
      quarterRound(x[0], x[4], x[8],  x[12]);
      quarterRound(x[1], x[5], x[9],  x[13]);
      quarterRound(x[2], x[6], x[10], x[14]);
      quarterRound(x[3], x[7], x[11], x[15]);
      quarterRound(x[0], x[5], x[10], x[15]);
      quarterRound(x[1], x[6], x[11], x[12]);
      quarterRound(x[2], x[7], x[8],  x[13]);
      quarterRound(x[3], x[4], x[9],  x[14]);
    }
    for (int i = 0; i < 16; ++i) store32_le(block + i * 4, x[i] + ctx[i]);
    ctx[12]++;

    usz take = (len < 64) ? len : 64;
    for (usz i = 0; i < take; ++i) out[i] = (in ? in[i] : 0) ^ block[i];
    if (in) in += take;
    out += take;
    len -= take;
  }
}

} // namespace Ksee

