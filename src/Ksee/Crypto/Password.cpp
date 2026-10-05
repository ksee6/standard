/**
 * @file Password.cpp
 * @brief Standalone Argon2id password-based key derivation function implementation.
 */

#include "../../../include/Ksee/Crypto/Password.hpp"
#include "../../../include/Ksee/Types.hpp"
#include "../../../include/Ksee/Array.hpp"
#include <string.h>

namespace Ksee {

// ---------------------------------------------------------------------------
// Blake2b internal implementation for Argon2
// ---------------------------------------------------------------------------

struct Blake2bCtx {
    u64 h[8];
    u64 t[2];
    u64 f[2];
    u8 buf[128];
    usz buflen;
    usz outlen;
};

static const u64 blake2b_iv[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
    0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

static const u8 blake2b_sigma[12][16] = {
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
    { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
    {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
    {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
    {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
    { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
    { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
    {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
    { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }
};

static inline u64 load64_le(const u8 *p) {
    return (u64)p[0]        | ((u64)p[1] << 8)  | ((u64)p[2] << 16) | ((u64)p[3] << 24) |
           ((u64)p[4] << 32)| ((u64)p[5] << 40) | ((u64)p[6] << 48) | ((u64)p[7] << 56);
}

static inline void store64_le(u8 *p, u64 v) {
    p[0] = (u8)v;       p[1] = (u8)(v >> 8);  p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24);
    p[4] = (u8)(v >> 32);p[5] = (u8)(v >> 40); p[6] = (u8)(v >> 48); p[7] = (u8)(v >> 56);
}

static inline void store32_le(u8 *p, u32 v) {
    p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24);
}

static inline u64 rotr64_pw(u64 w, int c) { return (w >> c) | (w << (64 - c)); }

static void blake2b_compress_internal(Blake2bCtx *ctx, const u8 block[128]) {
    u64 m[16];
    for (int i = 0; i < 16; ++i) m[i] = load64_le(block + i * 8);

    u64 v[16];
    for (int i = 0; i < 8; ++i) v[i] = ctx->h[i];
    v[8]  = blake2b_iv[0];
    v[9]  = blake2b_iv[1];
    v[10] = blake2b_iv[2];
    v[11] = blake2b_iv[3];
    v[12] = blake2b_iv[4] ^ ctx->t[0];
    v[13] = blake2b_iv[5] ^ ctx->t[1];
    v[14] = blake2b_iv[6] ^ ctx->f[0];
    v[15] = blake2b_iv[7] ^ ctx->f[1];

#define G_B2B(r, i, a, b, c, d) do { \
    a = a + b + m[blake2b_sigma[r][2*i]]; \
    d = rotr64_pw(d ^ a, 32); \
    c = c + d; \
    b = rotr64_pw(b ^ c, 24); \
    a = a + b + m[blake2b_sigma[r][2*i + 1]]; \
    d = rotr64_pw(d ^ a, 16); \
    c = c + d; \
    b = rotr64_pw(b ^ c, 63); } while (0)

    for (int r = 0; r < 12; ++r) {
        G_B2B(r, 0, v[0], v[4], v[8],  v[12]);
        G_B2B(r, 1, v[1], v[5], v[9],  v[13]);
        G_B2B(r, 2, v[2], v[6], v[10], v[14]);
        G_B2B(r, 3, v[3], v[7], v[11], v[15]);
        G_B2B(r, 4, v[0], v[5], v[10], v[15]);
        G_B2B(r, 5, v[1], v[6], v[11], v[12]);
        G_B2B(r, 6, v[2], v[7], v[8],  v[13]);
        G_B2B(r, 7, v[3], v[4], v[9],  v[14]);
    }
#undef G_B2B

    for (int i = 0; i < 8; ++i) ctx->h[i] ^= v[i] ^ v[i + 8];
}

static void blake2b_inc_counter(Blake2bCtx *ctx, u64 inc) {
    ctx->t[0] += inc;
    if (ctx->t[0] < inc) ctx->t[1]++;
}

static void blake2b_init_internal(Blake2bCtx *ctx, usz outlen) {
    for (int i = 0; i < 8; ++i) ctx->h[i] = blake2b_iv[i];
    ctx->h[0] ^= 0x01010000 ^ (u64)outlen;
    ctx->t[0] = ctx->t[1] = 0;
    ctx->f[0] = ctx->f[1] = 0;
    ctx->buflen = 0;
    ctx->outlen = outlen;
    for (int i = 0; i < 128; ++i) ctx->buf[i] = 0;
}

static void blake2b_update_internal(Blake2bCtx *ctx, const void *pin, usz inlen) {
    const u8 *in = (const u8*)pin;
    while (inlen > 0) {
        if (ctx->buflen == 128) {
            blake2b_inc_counter(ctx, 128);
            blake2b_compress_internal(ctx, ctx->buf);
            ctx->buflen = 0;
        }
        usz want = 128 - ctx->buflen;
        usz take = (inlen < want) ? inlen : want;
        for (usz i = 0; i < take; ++i) ctx->buf[ctx->buflen + i] = in[i];
        ctx->buflen += take;
        in += take;
        inlen -= take;
    }
}

static void blake2b_final_internal(Blake2bCtx *ctx, void *pout) {
    blake2b_inc_counter(ctx, ctx->buflen);
    ctx->f[0] = ~0ULL;
    for (usz i = ctx->buflen; i < 128; ++i) ctx->buf[i] = 0;
    blake2b_compress_internal(ctx, ctx->buf);

    u8 outbuf[64];
    for (int i = 0; i < 8; ++i) store64_le(outbuf + i * 8, ctx->h[i]);
    u8 *dst = (u8*)pout;
    for (usz i = 0; i < ctx->outlen; ++i) dst[i] = outbuf[i];
}

// ---------------------------------------------------------------------------
// Constants & RFC 9106 Argon2
// ---------------------------------------------------------------------------

static const usz ARGON2_BLOCK_SIZE   = 1024;
static const usz ARGON2_QWORDS       = ARGON2_BLOCK_SIZE / 8; // 128
static const usz ARGON2_SYNC_POINTS  = 4;

static void blake2b_long(u8 *out, u32 outlen, const u8 *in, usz inlen) {
    if (outlen <= 64) {
        Blake2bCtx ctx;
        u8 lenBuf[4];
        store32_le(lenBuf, outlen);
        blake2b_init_internal(&ctx, outlen);
        blake2b_update_internal(&ctx, lenBuf, 4);
        blake2b_update_internal(&ctx, in, inlen);
        blake2b_final_internal(&ctx, out);
        return;
    }

    u8 A[64];
    {
        Blake2bCtx ctx;
        u8 lenBuf[4];
        store32_le(lenBuf, outlen);
        blake2b_init_internal(&ctx, 64);
        blake2b_update_internal(&ctx, lenBuf, 4);
        blake2b_update_internal(&ctx, in, inlen);
        blake2b_final_internal(&ctx, A);
    }

    u32 written = 0;
    for (u32 i = 0; i < 32; i++) out[written++] = A[i];

    u32 r = (outlen + 31) / 32 - 2;
    for (u32 i = 0; i < r && written + 32 <= outlen; i++) {
        u8 B[64];
        Blake2bCtx ctx;
        blake2b_init_internal(&ctx, 64);
        blake2b_update_internal(&ctx, A, 64);
        blake2b_final_internal(&ctx, B);
        for (u32 j = 0; j < 32; j++) out[written++] = B[j];
        for (u32 j = 0; j < 64; j++) A[j] = B[j];
    }

    u32 remaining = outlen - written;
    if (remaining > 0) {
        Blake2bCtx ctx;
        u32 finalLen = remaining < 64 ? remaining : 64;
        blake2b_init_internal(&ctx, finalLen);
        blake2b_update_internal(&ctx, A, 64);
        u8 B[64];
        blake2b_final_internal(&ctx, B);
        for (u32 i = 0; i < remaining; i++) out[written++] = B[i];
    }
}

#define G_MIX(a, b, c, d) \
    a = a + b + 2ULL * (u64)(u32)(a) * (u64)(u32)(b); \
    d = rotr64_pw(d ^ a, 32); \
    c = c + d + 2ULL * (u64)(u32)(c) * (u64)(u32)(d); \
    b = rotr64_pw(b ^ c, 24); \
    a = a + b + 2ULL * (u64)(u32)(a) * (u64)(u32)(b); \
    d = rotr64_pw(d ^ a, 16); \
    c = c + d + 2ULL * (u64)(u32)(c) * (u64)(u32)(d); \
    b = rotr64_pw(b ^ c, 63);

static void blake2b_perm(u64 v[16]) {
    G_MIX(v[0],  v[4],  v[8],  v[12])
    G_MIX(v[1],  v[5],  v[9],  v[13])
    G_MIX(v[2],  v[6],  v[10], v[14])
    G_MIX(v[3],  v[7],  v[11], v[15])
    G_MIX(v[0],  v[5],  v[10], v[15])
    G_MIX(v[1],  v[6],  v[11], v[12])
    G_MIX(v[2],  v[7],  v[8],  v[13])
    G_MIX(v[3],  v[4],  v[9],  v[14])
}

static void argon2_compress(u64 *Z, const u64 *X, const u64 *Y) {
    u64 R[ARGON2_QWORDS];
    for (usz i = 0; i < ARGON2_QWORDS; i++) R[i] = X[i] ^ Y[i];

    u64 Q[ARGON2_QWORDS];
    for (usz i = 0; i < ARGON2_QWORDS; i++) Q[i] = R[i];

    for (usz i = 0; i < 8; i++) {
        u64 v[16];
        for (usz j = 0; j < 16; j++) v[j] = Q[i * 16 + j];
        blake2b_perm(v);
        for (usz j = 0; j < 16; j++) Q[i * 16 + j] = v[j];
    }

    for (usz j = 0; j < 8; j++) {
        u64 col[16];
        for (usz i = 0; i < 16; i++) col[i] = Q[(i % 8) * 16 + j * 2 + (i / 8)];
        blake2b_perm(col);
        for (usz i = 0; i < 16; i++) Q[(i % 8) * 16 + j * 2 + (i / 8)] = col[i];
    }

    for (usz i = 0; i < ARGON2_QWORDS; i++) Z[i] = Q[i] ^ R[i];
}

struct RefPos {
    u32 l;
    u32 z;
};

static RefPos argon2_index(const u64 *block, u32 pass, u32 lane,
                           u32 slice, u32 m_prime, u32 lanes,
                           u32 seg_len, u32 idx, bool is_argon2id) {
    bool data_independent = (pass == 0 && slice < ARGON2_SYNC_POINTS / 2);
    if (is_argon2id && !data_independent) {
        u64 J1 = block[0];
        u32 ref_lane = (u32)((J1 >> 32) % (u64)lanes);
        if (pass == 0) ref_lane = lane;

        u64 J1lo = J1 & 0xFFFFFFFFULL;
        u32 ref_area_size;
        if (pass == 0) {
            if (slice == 0) ref_area_size = idx - 1;
            else if (ref_lane == lane) ref_area_size = slice * seg_len + idx - 1;
            else ref_area_size = slice * seg_len - (idx == 0 ? 1 : 0);
        } else {
            if (ref_lane == lane) ref_area_size = m_prime - seg_len + idx - 1;
            else ref_area_size = m_prime - seg_len - (idx == 0 ? 1 : 0);
        }

        u64 rel_pos = (J1lo * J1lo) >> 32;
        rel_pos = (u64)ref_area_size - 1 - ((u64)ref_area_size * rel_pos >> 32);

        u32 start_pos = 0;
        if (pass != 0 && slice != ARGON2_SYNC_POINTS - 1)
            start_pos = (slice + 1) * seg_len;

        u32 z = (u32)(((u64)start_pos + rel_pos) % (u64)m_prime / lanes);
        return {ref_lane, z};
    } else {
        u32 ref_lane = lane;
        u32 z = (idx == 0) ? 0 : (idx - 1) % (seg_len > 0 ? seg_len : 1);
        return {ref_lane, z};
    }
}

static String argon2id_raw(const String &password, const String &salt,
                           u32 outlen, u32 m_cost, u32 t_cost, u32 parallelism) {
    if (outlen < 4 || outlen > 0x3FFFFFFF) return String();
    if (m_cost < 8 * parallelism) return String();
    if (t_cost < 1 || parallelism < 1) return String();

    const u32 lanes = parallelism;
    const u32 m_prime = (m_cost / (4 * lanes)) * 4 * lanes;
    if (m_prime < 8 * lanes) return String();
    const u32 seg_len = m_prime / (lanes * ARGON2_SYNC_POINTS);
    const usz total_blocks = (usz)m_prime;

    Array<u64> mem;
    for (usz i = 0; i < total_blocks * ARGON2_QWORDS; i++) mem.push(0);

    u8 h0_buf[72];
    {
        Blake2bCtx ctx;
        blake2b_init_internal(&ctx, 64);

        u8 tmp[4];
        store32_le(tmp, parallelism); blake2b_update_internal(&ctx, tmp, 4);
        store32_le(tmp, outlen);      blake2b_update_internal(&ctx, tmp, 4);
        store32_le(tmp, m_cost);      blake2b_update_internal(&ctx, tmp, 4);
        store32_le(tmp, t_cost);      blake2b_update_internal(&ctx, tmp, 4);
        store32_le(tmp, 19);          blake2b_update_internal(&ctx, tmp, 4);
        store32_le(tmp, 2);           blake2b_update_internal(&ctx, tmp, 4);

        store32_le(tmp, (u32)password.size()); blake2b_update_internal(&ctx, tmp, 4);
        blake2b_update_internal(&ctx, (const u8 *)password.data(), password.size());

        store32_le(tmp, (u32)salt.size()); blake2b_update_internal(&ctx, tmp, 4);
        blake2b_update_internal(&ctx, (const u8 *)salt.data(), salt.size());

        store32_le(tmp, 0); blake2b_update_internal(&ctx, tmp, 4);
        store32_le(tmp, 0); blake2b_update_internal(&ctx, tmp, 4);

        blake2b_final_internal(&ctx, h0_buf);
    }

    for (u32 lane = 0; lane < lanes; lane++) {
        for (u32 col = 0; col < 2; col++) {
            u8 input[72];
            for (int i = 0; i < 64; i++) input[i] = h0_buf[i];
            store32_le(input + 64, col);
            store32_le(input + 68, lane);

            u8 block_out[ARGON2_BLOCK_SIZE];
            blake2b_long(block_out, ARGON2_BLOCK_SIZE, input, 72);

            u64 *B = mem.data() + ((u64)lane * (m_prime / lanes) + col) * ARGON2_QWORDS;
            for (usz i = 0; i < ARGON2_QWORDS; i++)
                B[i] = load64_le(block_out + i * 8);
        }
    }

    for (u32 pass = 0; pass < t_cost; pass++) {
        for (u32 slice = 0; slice < ARGON2_SYNC_POINTS; slice++) {
            for (u32 lane = 0; lane < lanes; lane++) {
                const u32 lane_blocks = m_prime / lanes;
                const u32 seg_start = slice * seg_len;

                for (u32 idx = 0; idx < seg_len; idx++) {
                    u32 cur_col = seg_start + idx;
                    if (pass == 0 && slice == 0 && idx < 2) continue;

                    u32 prev_col = (cur_col == 0) ? lane_blocks - 1 : cur_col - 1;
                    u64 *cur  = mem.data() + ((u64)lane * lane_blocks + cur_col)  * ARGON2_QWORDS;
                    u64 *prev = mem.data() + ((u64)lane * lane_blocks + prev_col) * ARGON2_QWORDS;

                    RefPos ref = argon2_index(prev, pass, lane, slice,
                                              m_prime, lanes, seg_len, idx,
                                              true);
                    ref.l = ref.l % lanes;
                    ref.z = ref.z % (m_prime / lanes);

                    u64 *ref_block = mem.data() + ((u64)ref.l * lane_blocks + ref.z) * ARGON2_QWORDS;

                    u64 tmp[ARGON2_QWORDS];
                    argon2_compress(tmp, prev, ref_block);

                    if (pass == 0) {
                        for (usz i = 0; i < ARGON2_QWORDS; i++) cur[i] = tmp[i];
                    } else {
                        for (usz i = 0; i < ARGON2_QWORDS; i++) cur[i] ^= tmp[i];
                    }
                }
            }
        }
    }

    u64 C[ARGON2_QWORDS];
    const u32 lane_blocks = m_prime / lanes;
    u64 *last0 = mem.data() + ((u64)0 * lane_blocks + lane_blocks - 1) * ARGON2_QWORDS;
    for (usz i = 0; i < ARGON2_QWORDS; i++) C[i] = last0[i];

    for (u32 lane = 1; lane < lanes; lane++) {
        u64 *last_l = mem.data() + ((u64)lane * lane_blocks + lane_blocks - 1) * ARGON2_QWORDS;
        for (usz i = 0; i < ARGON2_QWORDS; i++) C[i] ^= last_l[i];
    }

    u8 C_bytes[ARGON2_BLOCK_SIZE];
    for (usz i = 0; i < ARGON2_QWORDS; i++) store64_le(C_bytes + i * 8, C[i]);

    String result;
    u8* outbuf = new u8[outlen];
    blake2b_long(outbuf, outlen, C_bytes, ARGON2_BLOCK_SIZE);
    for (usz i = 0; i < outlen; ++i) result.push(outbuf[i]);
    delete[] outbuf;

    return result;
}

String pbkdf(const String &input, u64 salt, const String &domain,
             usz outputLength, u32 kilobytes, u32 time) {
    String saltBytes;
    for (int i = 0; i < 8; ++i) {
        saltBytes.push((u8)((salt >> (i * 8)) & 0xFF));
    }
    saltBytes += domain;

    return argon2id_raw(input, saltBytes, (u32)outputLength, kilobytes, time, 1);
}

} // namespace Ksee
