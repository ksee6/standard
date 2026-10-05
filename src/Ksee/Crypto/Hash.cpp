/**
 * @file Hash.cpp
 * @brief High-performance standalone BLAKE3 hash and KDF implementation.
 */

#include "../../../include/Ksee/Crypto/Hash.hpp"
#include <string.h>

namespace Ksee {

// ---------------------------------------------------------------------------
// BLAKE3 Constants and Enums
// ---------------------------------------------------------------------------

static const u32 BLAKE3_IV[8] = {
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
    0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

enum Blake3Flags : u32 {
    CHUNK_START         = 1 << 0,
    CHUNK_END           = 1 << 1,
    PARENT              = 1 << 2,
    ROOT                = 1 << 3,
    KEYED_HASH          = 1 << 4,
    DERIVE_KEY_CONTEXT  = 1 << 5,
    DERIVE_KEY_MATERIAL = 1 << 6
};

static const u8 MSG_PERMUTATION[16] = {
    2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static inline void g(u32 &a, u32 &b, u32 &c, u32 &d, u32 x, u32 y) {
    a = a + b + x;
    d = ROTR32(d ^ a, 16);
    c = c + d;
    b = ROTR32(b ^ c, 12);
    a = a + b + y;
    d = ROTR32(d ^ a, 8);
    c = c + d;
    b = ROTR32(b ^ c, 7);
}

static inline void round_fn(u32 state[16], const u32 m[16]) {
    g(state[0], state[4], state[8],  state[12], m[0], m[1]);
    g(state[1], state[5], state[9],  state[13], m[2], m[3]);
    g(state[2], state[6], state[10], state[14], m[4], m[5]);
    g(state[3], state[7], state[11], state[15], m[6], m[7]);

    g(state[0], state[5], state[10], state[15], m[8],  m[9]);
    g(state[1], state[6], state[11], state[12], m[10], m[11]);
    g(state[2], state[7], state[8],  state[13], m[12], m[13]);
    g(state[3], state[4], state[9],  state[14], m[14], m[15]);
}

static inline void compress_pre(u32 state[16], const u32 cv[8], const u32 block[16],
                                u32 block_len, u64 counter, u32 flags) {
    for (int i = 0; i < 8; ++i) state[i] = cv[i];
    state[8]  = BLAKE3_IV[0];
    state[9]  = BLAKE3_IV[1];
    state[10] = BLAKE3_IV[2];
    state[11] = BLAKE3_IV[3];
    state[12] = (u32)(counter & 0xFFFFFFFF);
    state[13] = (u32)(counter >> 32);
    state[14] = block_len;
    state[15] = flags;

    u32 m[16];
    for (int i = 0; i < 16; ++i) m[i] = block[i];

    for (int r = 0; r < 7; ++r) {
        round_fn(state, m);
        if (r < 6) {
            u32 next_m[16];
            for (int i = 0; i < 16; ++i) {
                next_m[i] = m[MSG_PERMUTATION[i]];
            }
            for (int i = 0; i < 16; ++i) {
                m[i] = next_m[i];
            }
        }
    }
}

static inline void compress_in_place(u32 cv[8], const u32 block[16], u32 block_len,
                                     u64 counter, u32 flags) {
    u32 state[16];
    compress_pre(state, cv, block, block_len, counter, flags);
    for (int i = 0; i < 8; ++i) {
        cv[i] = state[i] ^ state[i + 8];
    }
}

static inline void compress_xof(const u32 cv[8], const u32 block[16], u32 block_len,
                                u64 counter, u32 flags, u8 out[64]) {
    u32 state[16];
    compress_pre(state, cv, block, block_len, counter, flags);
    for (int i = 0; i < 8; ++i) {
        u32 w = state[i] ^ state[i + 8];
        out[i * 4 + 0] = (u8)(w & 0xFF);
        out[i * 4 + 1] = (u8)((w >> 8) & 0xFF);
        out[i * 4 + 2] = (u8)((w >> 16) & 0xFF);
        out[i * 4 + 3] = (u8)((w >> 24) & 0xFF);
    }
    for (int i = 0; i < 8; ++i) {
        u32 w = state[i + 8] ^ cv[i];
        out[(i + 8) * 4 + 0] = (u8)(w & 0xFF);
        out[(i + 8) * 4 + 1] = (u8)((w >> 8) & 0xFF);
        out[(i + 8) * 4 + 2] = (u8)((w >> 16) & 0xFF);
        out[(i + 8) * 4 + 3] = (u8)((w >> 24) & 0xFF);
    }
}

static inline u32 load32_le(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static inline void load_block(const u8 *input, usz len, u32 block[16]) {
    for (int i = 0; i < 16; ++i) block[i] = 0;
    usz words = len / 4;
    for (usz i = 0; i < words; ++i) {
        block[i] = load32_le(input + i * 4);
    }
    usz remainder = len % 4;
    if (remainder > 0) {
        u32 last_word = 0;
        const u8 *rem_p = input + words * 4;
        for (usz i = 0; i < remainder; ++i) {
            last_word |= ((u32)rem_p[i]) << (i * 8);
        }
        block[words] = last_word;
    }
}

static inline usz left_subtree_len(usz input_len) {
    usz full_chunks = (input_len - 1) / 1024;
    usz p = 1;
    while ((p << 1) <= full_chunks) {
        p <<= 1;
    }
    return p * 1024;
}

// ---------------------------------------------------------------------------
// Chunk and Subtree Hashing
// ---------------------------------------------------------------------------

static void hash_chunk(const u8 *input, usz len, const u32 key[8], u64 chunk_counter,
                       u32 flags, bool is_root, u8 *out, usz out_len) {
    u32 cv[8];
    for (int i = 0; i < 8; ++i) cv[i] = key[i];

    usz num_blocks = (len == 0) ? 1 : ((len + 63) / 64);
    for (usz b = 0; b < num_blocks; ++b) {
        u32 block[16];
        usz block_offset = b * 64;
        usz cur_block_len = (len == 0) ? 0 : ((b == num_blocks - 1) ? (len - block_offset) : 64);
        load_block(input + block_offset, cur_block_len, block);

        u32 block_flags = flags;
        if (b == 0) block_flags |= CHUNK_START;
        if (b == num_blocks - 1) {
            block_flags |= CHUNK_END;
            if (is_root) {
                block_flags |= ROOT;
                u64 output_counter = 0;
                usz written = 0;
                while (written < out_len) {
                    u8 xof[64];
                    compress_xof(cv, block, (u32)cur_block_len, output_counter, block_flags, xof);
                    usz to_copy = (out_len - written < 64) ? (out_len - written) : 64;
                    memcpy(out + written, xof, to_copy);
                    written += to_copy;
                    output_counter++;
                }
                return;
            }
        }
        compress_in_place(cv, block, (u32)cur_block_len, chunk_counter, block_flags);
    }

    for (int i = 0; i < 8; ++i) {
        out[i * 4 + 0] = (u8)(cv[i] & 0xFF);
        out[i * 4 + 1] = (u8)((cv[i] >> 8) & 0xFF);
        out[i * 4 + 2] = (u8)((cv[i] >> 16) & 0xFF);
        out[i * 4 + 3] = (u8)((cv[i] >> 24) & 0xFF);
    }
}

static void hash_subtree(const u8 *input, usz len, const u32 key[8], u64 chunk_counter,
                         u32 flags, bool is_root, u8 *out, usz out_len) {
    if (len <= 1024) {
        hash_chunk(input, len, key, chunk_counter, flags, is_root, out, out_len);
        return;
    }

    usz left_len = left_subtree_len(len);
    usz right_len = len - left_len;
    u64 right_chunk_counter = chunk_counter + (u64)(left_len / 1024);

    u8 left_cv[32];
    u8 right_cv[32];

    hash_subtree(input, left_len, key, chunk_counter, flags, false, left_cv, 32);
    hash_subtree(input + left_len, right_len, key, right_chunk_counter, flags, false, right_cv, 32);

    u32 parent_block[16];
    for (int i = 0; i < 8; ++i) parent_block[i] = load32_le(left_cv + i * 4);
    for (int i = 0; i < 8; ++i) parent_block[8 + i] = load32_le(right_cv + i * 4);

    u32 parent_flags = flags | PARENT;
    if (is_root) {
        parent_flags |= ROOT;
        u64 output_counter = 0;
        usz written = 0;
        while (written < out_len) {
            u8 xof[64];
            compress_xof(key, parent_block, 64, output_counter, parent_flags, xof);
            usz to_copy = (out_len - written < 64) ? (out_len - written) : 64;
            memcpy(out + written, xof, to_copy);
            written += to_copy;
            output_counter++;
        }
    } else {
        u32 cv[8];
        for (int i = 0; i < 8; ++i) cv[i] = key[i];
        compress_in_place(cv, parent_block, 64, 0, parent_flags);
        for (int i = 0; i < 8; ++i) {
            out[i * 4 + 0] = (u8)(cv[i] & 0xFF);
            out[i * 4 + 1] = (u8)((cv[i] >> 8) & 0xFF);
            out[i * 4 + 2] = (u8)((cv[i] >> 16) & 0xFF);
            out[i * 4 + 3] = (u8)((cv[i] >> 24) & 0xFF);
        }
    }
}

// Dynamic context used for tree calculation when payload > 1 KiB
struct Blake3TreeContext {
    u32 key[8];
    u32 flags;
    u64 chunk_counter;

    Blake3TreeContext(const u32 k[8], u32 f, u64 c) : flags(f), chunk_counter(c) {
        for (int i = 0; i < 8; ++i) key[i] = k[i];
    }
};

static void blake3_dispatch(const u8 *input, usz length, const u32 key[8], u32 flags,
                            u8 *out, usz out_len) {
    if (length <= 1024) {
        // Zero static slots, zero dynamic allocation for <= 1 KiB payloads
        hash_chunk(input, length, key, 0, flags, true, out, out_len);
    } else {
        // Dynamic auto-allocation for payloads > 1 KiB
        Blake3TreeContext *ctx = new Blake3TreeContext(key, flags, 0);
        hash_subtree(input, length, ctx->key, ctx->chunk_counter, ctx->flags, true, out, out_len);
        delete ctx;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void hash(const char *input, usz length, usz outputPtr, usz outputLength) {
    if (outputLength == 0 || outputPtr == 0) return;
    const u8 *in_p = (const u8 *)input;
    u8 *out_p = (u8 *)outputPtr;
    blake3_dispatch(in_p, length, BLAKE3_IV, 0, out_p, outputLength);
}

String hash(const String &input, usz outputLength) {
    String res;
    if (outputLength == 0) return res;
    res.allocate(outputLength);
    hash((const char *)input.data(), input.size(), (usz)res.data(), outputLength);
    return res;
}

String kdf(const String &input, u64 salt, const String &domain, usz outputLength) {
    // Stage 1: Context key derivation
    u8 context_key_bytes[32];
    blake3_dispatch((const u8 *)domain.data(), domain.size(), BLAKE3_IV, DERIVE_KEY_CONTEXT,
                    context_key_bytes, 32);

    u32 context_key[8];
    for (int i = 0; i < 8; ++i) {
        context_key[i] = load32_le(context_key_bytes + i * 4);
    }

    // Stage 2: Key material derivation
    String material;
    if (salt != 0) {
        for (int i = 0; i < 8; ++i) {
            material.push((u8)((salt >> (i * 8)) & 0xFF));
        }
    }
    material += input;

    String res;
    if (outputLength == 0) return res;
    res.allocate(outputLength);
    blake3_dispatch((const u8 *)material.data(), material.size(), context_key,
                    DERIVE_KEY_MATERIAL, (u8 *)res.data(), outputLength);
    return res;
}

} // namespace Ksee
