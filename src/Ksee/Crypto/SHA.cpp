/**
 * @file SHA256.cpp
 * @brief SHA-256 implementation without standard library dependencies.
 */

#include "../../../include/Ksee/Crypto/SHA.hpp"
#include <string.h>

namespace Ksee {

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define Ch(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define Sigma0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define Sigma1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define sigma0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define sigma1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

const u32 SHA256::K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

SHA256::SHA256() {
    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;
    count = 0;
}

static inline u32 read32BE(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static inline void write32BE(u8* p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

void SHA256::transform(const u8 block[64]) {
    u32 w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = read32BE(block + i * 4);
    }
    for (int i = 16; i < 64; i++) {
        w[i] = sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16];
    }

    u32 a = state[0];
    u32 b = state[1];
    u32 c = state[2];
    u32 d = state[3];
    u32 e = state[4];
    u32 f = state[5];
    u32 g = state[6];
    u32 h = state[7];

    for (int i = 0; i < 64; i++) {
        u32 t1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + w[i];
        u32 t2 = Sigma0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void SHA256::update(const void* data, usz len) {
    const u8* p = (const u8*)data;
    usz bufferIdx = (count / 8) % BlockSize;
    count += (u64)len * 8;

    usz partLen = BlockSize - bufferIdx;
    usz i = 0;

    if (len >= partLen) {
        for (usz j = 0; j < partLen; ++j) buffer[bufferIdx + j] = p[j];
        transform(buffer);
        for (i = partLen; i + BlockSize <= len; i += BlockSize) {
            transform(p + i);
        }
        bufferIdx = 0;
    }

    for (usz j = 0; j < len - i; ++j) buffer[bufferIdx + j] = p[i + j];
}

void SHA256::update(const String& str) {
    update(str.data(), str.length());
}

void SHA256::final(u8 digest[DigestSize]) {
    u8 finalCount[8];
    for (int i = 0; i < 8; i++) {
        finalCount[i] = (u8)((count >> ((7 - i) * 8)) & 0xff);
    }

    u8 pad[64];
    pad[0] = 0x80;
    for (int i = 1; i < 64; ++i) pad[i] = 0;

    usz bufferIdx = (count / 8) % BlockSize;
    usz padLen = (bufferIdx < 56) ? (56 - bufferIdx) : (120 - bufferIdx);
    update(pad, padLen);
    update(finalCount, 8);

    for (int i = 0; i < 8; i++) {
        write32BE(digest + i * 4, state[i]);
    }
}

String SHA256::final() {
    u8 digest[DigestSize];
    final(digest);
    String res;
    res.pushEach(digest, DigestSize);
    return res;
}

String SHA256::hash(const String& input) {
    SHA256 ctx;
    ctx.update(input);
    return ctx.final();
}

String SHA256::hash(const void* data, usz len) {
    SHA256 ctx;
    ctx.update(data, len);
    return ctx.final();
}

void SHA256::hash(const void* data, usz len, u8 digest[DigestSize]) {
    SHA256 ctx;
    ctx.update(data, len);
    ctx.final(digest);
}

/**
 * @file SHA512.cpp
 * @brief High-performance standalone implementation of SHA-512.
 */




const u64 SHA512::K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static inline u64 rotr64(u64 x, unsigned int n) {
    return (x >> n) | (x << (64 - n));
}

static inline u64 load64_be(const u8* p) {
    return ((u64)p[0] << 56) | ((u64)p[1] << 48) | ((u64)p[2] << 40) | ((u64)p[3] << 32) |
           ((u64)p[4] << 24) | ((u64)p[5] << 16) | ((u64)p[6] << 8)  | (u64)p[7];
}

static inline void store64_be(u8* p, u64 v) {
    p[0] = (u8)(v >> 56);
    p[1] = (u8)(v >> 48);
    p[2] = (u8)(v >> 40);
    p[3] = (u8)(v >> 32);
    p[4] = (u8)(v >> 24);
    p[5] = (u8)(v >> 16);
    p[6] = (u8)(v >> 8);
    p[7] = (u8)v;
}

static inline u64 Ch512(u64 x, u64 y, u64 z)  { return (x & y) ^ (~x & z); }
static inline u64 Maj512(u64 x, u64 y, u64 z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline u64 Sigma0_512(u64 x)           { return rotr64(x, 28) ^ rotr64(x, 34) ^ rotr64(x, 39); }
static inline u64 Sigma1_512(u64 x)           { return rotr64(x, 14) ^ rotr64(x, 18) ^ rotr64(x, 41); }
static inline u64 sigma0_512(u64 x)           { return rotr64(x, 1) ^ rotr64(x, 8) ^ (x >> 7); }
static inline u64 sigma1_512(u64 x)           { return rotr64(x, 19) ^ rotr64(x, 61) ^ (x >> 6); }

SHA512::SHA512() {
    state[0] = 0x6a09e667f3bcc908ULL;
    state[1] = 0xbb67ae8584caa73bULL;
    state[2] = 0x3c6ef372fe94f82bULL;
    state[3] = 0xa54ff53a5f1d36f1ULL;
    state[4] = 0x510e527fade682d1ULL;
    state[5] = 0x9b05688c2b3e6c1fULL;
    state[6] = 0x1f83d9abfb41bd6bULL;
    state[7] = 0x5be0cd19137e2179ULL;
    count[0] = 0;
    count[1] = 0;
}

void SHA512::transform(const u8 block[128]) {
    u64 W[80];
    for (int i = 0; i < 16; ++i) {
        W[i] = load64_be(block + i * 8);
    }
    for (int i = 16; i < 80; ++i) {
        W[i] = sigma1_512(W[i - 2]) + W[i - 7] + sigma0_512(W[i - 15]) + W[i - 16];
    }

    u64 a = state[0], b = state[1], c = state[2], d = state[3];
    u64 e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 80; ++i) {
        u64 T1 = h + Sigma1_512(e) + Ch512(e, f, g) + K[i] + W[i];
        u64 T2 = Sigma0_512(a) + Maj512(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void SHA512::update(const void* data, usz len) {
    if (!data || len == 0) return;
    const u8* src = reinterpret_cast<const u8*>(data);

    u64 bitLen = static_cast<u64>(len) * 8;
    u64 oldLow = count[0];
    count[0] += bitLen;
    if (count[0] < oldLow) count[1]++;
    count[1] += (static_cast<u64>(len) >> 61);

    usz index = (oldLow >> 3) & 0x7f;
    usz partLen = 128 - index;

    usz i = 0;
    if (len >= partLen) {
        memcpy(&buffer[index], src, partLen);
        transform(buffer);
        for (i = partLen; i + 128 <= len; i += 128) {
            transform(src + i);
        }
        index = 0;
    }

    memcpy(&buffer[index], src + i, len - i);
}

void SHA512::update(const String& str) {
    update(str.data(), str.size());
}

void SHA512::final(u8 digest[64]) {
    u8 pad[128];
    memset(pad, 0, 128);
    pad[0] = 0x80;

    usz index = (count[0] >> 3) & 0x7f;
    usz padLen = (index < 112) ? (112 - index) : (240 - index);

    u64 bitsHigh = count[1];
    u64 bitsLow  = count[0];

    update(pad, padLen);

    u8 lenBytes[16];
    store64_be(lenBytes, bitsHigh);
    store64_be(lenBytes + 8, bitsLow);

    // Write final length directly to buffer and transform
    memcpy(&buffer[112], lenBytes, 16);
    transform(buffer);

    for (int i = 0; i < 8; ++i) {
        store64_be(digest + i * 8, state[i]);
    }
}

String SHA512::final() {
    u8 digest[64];
    final(digest);
    String out;
    for (int i = 0; i < 64; ++i) out.push(digest[i]);
    return out;
}

String SHA512::hash(const String& input) {
    return hash(input.data(), input.size());
}

String SHA512::hash(const void* data, usz len) {
    SHA512 ctx;
    ctx.update(data, len);
    return ctx.final();
}

void SHA512::hash(const void* data, usz len, u8 digest[64]) {
    SHA512 ctx;
    ctx.update(data, len);
    ctx.final(digest);
}



} // namespace Ksee
