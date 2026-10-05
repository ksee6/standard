/**
 * @file AEAD.cpp
 * @brief RFC 8439 ChaCha12-Poly1305 AEAD implementation for Ksee.
 * Poly1305 is directly embedded in this file.
 */

#include "../../../include/Ksee/Crypto/AEAD.hpp"
#include "../../../include/Ksee/Crypto/Chacha.hpp"

namespace Ksee {

// ---------------------------------------------------------------------------
// Directly embedded Poly1305 implementation
// ---------------------------------------------------------------------------

static inline u64 load64_le(const u8 *p) {
  return ((u64)p[0]) | ((u64)p[1] << 8) | ((u64)p[2] << 16) | ((u64)p[3] << 24) |
         ((u64)p[4] << 32) | ((u64)p[5] << 40) | ((u64)p[6] << 48) | ((u64)p[7] << 56);
}

static inline void store64_le(u8 *p, u64 v) {
  p[0] = (u8)v;
  p[1] = (u8)(v >> 8);
  p[2] = (u8)(v >> 16);
  p[3] = (u8)(v >> 24);
  p[4] = (u8)(v >> 32);
  p[5] = (u8)(v >> 40);
  p[6] = (u8)(v >> 48);
  p[7] = (u8)(v >> 56);
}

typedef unsigned __int128 poly128;

static void poly1305_auth(u8 tag[16], const u8 *msg, usz len, const u8 key[32]) {
  u64 r0 = load64_le(key) & 0x0ffffffc0fffffffULL;
  u64 r1 = load64_le(key + 8) & 0x0ffffffc0ffffffcULL;
  u64 s0 = load64_le(key + 16);
  u64 s1 = load64_le(key + 24);

  u64 r1_prime = (r1 >> 2) * 5;
  u64 h0 = 0, h1 = 0, h2 = 0;

  while (len > 0) {
    usz take = (len < 16) ? len : 16;
    u8 block[17];
    for (int i = 0; i < 17; ++i) block[i] = 0;
    for (usz i = 0; i < take; ++i) block[i] = msg[i];
    block[take] = 1;

    u64 b0 = load64_le(block);
    u64 b1 = load64_le(block + 8);
    u64 b2 = block[16];

    poly128 c0 = (poly128)h0 + b0;
    h0 = (u64)c0;
    poly128 c1 = (poly128)h1 + b1 + (u64)(c0 >> 64);
    h1 = (u64)c1;
    h2 += b2 + (u64)(c1 >> 64);

    poly128 d0 = (poly128)h0 * r0 + (poly128)h1 * r1_prime;
    poly128 d1 = (poly128)h0 * r1 + (poly128)h1 * r0 + (poly128)h2 * r1_prime;
    poly128 d2 = (poly128)h2 * r0;

    poly128 d1_hi = (d1 >> 64) + d2;
    u64 d1_hi_64 = (u64)d1_hi;
    poly128 c = (poly128)d0 + (poly128)(d1_hi_64 >> 2) * 5;
    h0 = (u64)c;
    poly128 d1_lo = (d1 & 0xffffffffffffffffULL) + (c >> 64);
    h1 = (u64)d1_lo;
    h2 = (d1_hi_64 & 3) + (u64)(d1_lo >> 64);

    if (h2 >= 4) {
      u64 q = h2 >> 2;
      h2 &= 3;
      poly128 c2 = (poly128)h0 + (poly128)q * 5;
      h0 = (u64)c2;
      h1 += (u64)(c2 >> 64);
    }

    msg += take;
    len -= take;
  }

  u64 g0 = h0 + 5;
  u64 c0 = (g0 < h0) ? 1 : 0;
  u64 g1 = h1 + c0;
  u64 c1 = (g1 < h1) ? 1 : 0;
  u64 g2 = h2 + c1;

  u64 mask = (g2 >> 2) ? 0xFFFFFFFFFFFFFFFFULL : 0;
  h0 = (h0 & ~mask) | ((g0 - 5) & mask);
  h1 = (h1 & ~mask) | (g1 & mask);

  poly128 f0 = (poly128)h0 + s0;
  h0 = (u64)f0;
  poly128 f1 = (poly128)h1 + s1 + (u64)(f0 >> 64);
  h1 = (u64)f1;

  store64_le(tag, h0);
  store64_le(tag + 8, h1);
}

// ---------------------------------------------------------------------------
// AEAD methods
// ---------------------------------------------------------------------------

AEAD::AEAD(const String &str, usz headerLength) {
  if (str.size() < headerLength) {
    ok = false;
    return;
  }
  header = str.slice(0, (long long)headerLength);
  text = str.slice((long long)headerLength);
  hash = "";
  ok = true;
}

AEAD AEAD::fromStringVarHeader(const String &str) {
  AEAD ret;
  auto res = str.peekVarString(0);
  if (res.error || (usz)res.bytes > str.size()) {
    ret.ok = false;
    return ret;
  }
  ret.header = res.value;
  ret.text = str.slice((long long)res.bytes);
  ret.hash = "";
  ret.ok = true;
  return ret;
}

String AEAD::toString(usz headerLength) const {
  String out;
  if (header.size() < headerLength) {
    out += header;
    for (usz i = header.size(); i < headerLength; ++i) {
      out.push((u8)0);
    }
  } else if (header.size() > headerLength) {
    out += header.slice(0, (long long)headerLength);
  } else {
    out += header;
  }
  out += text;
  out += hash;
  return out;
}

String AEAD::toStringVarHeader() const {
  String out;
  out.pushVarString(header);
  out += text;
  out += hash;
  return out;
}

static String makeZeros(usz len) {
  String s;
  if (len == 0) return s;
  u8 *buf = new u8[len];
  for (usz i = 0; i < len; ++i) buf[i] = 0;
  s.pushEach(buf, len);
  delete[] buf;
  return s;
}

AEAD seal(const AEAD &plain, u256 key, u64 nonce, int rounds) {
  u512 block0 = chacha(key, nonce, (u8)rounds, 0);
  u8 oneTimeKey[32];
  for (int i = 0; i < 32; ++i) oneTimeKey[i] = block0[i];

  String ciphertext = crypt(plain.text, key, nonce, (u8)rounds);

  u64 adLen = plain.header.size();
  u64 cipherLen = ciphertext.size();
  usz adPad = (16 - (adLen % 16)) % 16;
  usz cipherPad = (16 - (cipherLen % 16)) % 16;

  String dataToAuth;
  dataToAuth += plain.header;
  dataToAuth += makeZeros(adPad);
  dataToAuth += ciphertext;
  dataToAuth += makeZeros(cipherPad);

  for (int i = 0; i < 8; ++i)
    dataToAuth.push((u8)((adLen >> (i * 8)) & 0xFF));
  for (int i = 0; i < 8; ++i)
    dataToAuth.push((u8)((cipherLen >> (i * 8)) & 0xFF));

  u8 tag[16];
  poly1305_auth(tag, (const u8*)dataToAuth.data(), dataToAuth.size(), oneTimeKey);

  String fullTag;
  fullTag.pushEach(tag, 16);

  AEAD sealed;
  sealed.text = ciphertext;
  sealed.header = plain.header;
  sealed.hash = fullTag;
  sealed.ok = true;
  return sealed;
}

AEAD open(const AEAD &sealed, u256 key, u64 nonce, int rounds) {
  AEAD fail;
  fail.ok = false;

  String ciphertext;
  String tag;
  if (sealed.hash.size() > 0) {
    ciphertext = sealed.text;
    tag = sealed.hash;
  } else {
    if (sealed.text.size() < 16) {
      return fail;
    }
    usz cipherLen = sealed.text.size() - 16;
    ciphertext = sealed.text.slice(0, (long long)cipherLen);
    tag = sealed.text.slice((long long)cipherLen);
  }

  if (tag.size() != 16 && tag.size() != 8) {
    return fail;
  }

  u512 block0 = chacha(key, nonce, (u8)rounds, 0);
  u8 oneTimeKey[32];
  for (int i = 0; i < 32; ++i) oneTimeKey[i] = block0[i];

  u64 adLen = sealed.header.size();
  u64 cipherLen = ciphertext.size();
  usz adPad = (16 - (adLen % 16)) % 16;
  usz cipherPad = (16 - (cipherLen % 16)) % 16;

  String dataToAuth;
  dataToAuth += sealed.header;
  dataToAuth += makeZeros(adPad);
  dataToAuth += ciphertext;
  dataToAuth += makeZeros(cipherPad);

  for (int i = 0; i < 8; ++i)
    dataToAuth.push((u8)((adLen >> (i * 8)) & 0xFF));
  for (int i = 0; i < 8; ++i)
    dataToAuth.push((u8)((cipherLen >> (i * 8)) & 0xFF));

  u8 expectedTagBuf[16];
  poly1305_auth(expectedTagBuf, (const u8*)dataToAuth.data(), dataToAuth.size(), oneTimeKey);

  String expectedTag;
  expectedTag.pushEach(expectedTagBuf, tag.size());

  if (!tag.constantTimeEquals(expectedTag, (int)tag.size())) {
    return fail;
  }

  String decrypted = crypt(ciphertext, key, nonce, (u8)rounds);

  AEAD plain;
  plain.text = decrypted;
  plain.header = sealed.header;
  plain.hash = tag;
  plain.ok = true;
  return plain;
}

} // namespace Ksee

