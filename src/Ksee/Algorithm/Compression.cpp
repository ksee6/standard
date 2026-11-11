/**
 * @file Compression.cpp
 * @brief High-performance standalone DEFLATE and INFLATE engine for Ksee.
 *        100% self-contained RFC 1951, RFC 1950 (zlib), and RFC 1952 (gzip) implementation.
 *        Zero external dependencies (no zlib or miniz required).
 */

#include "../../../include/Ksee/Algorithm/Compression.hpp"
#include <string.h>

namespace Ksee {
namespace Algorithm {

// =============================================================================
// DEFLATE / INFLATE Specification Tables & Constants
// =============================================================================

// Length base values and extra bits for codes 257..285
static const u16 LENGTH_BASE[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

static const u8 LENGTH_EXTRA[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

// Distance base values and extra bits for codes 0..29
static const u16 DISTANCE_BASE[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

static const u8 DISTANCE_EXTRA[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

// Order of code length alphabet in dynamic header
static const u8 CL_ORDER[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

// Fixed Huffman tables for BTYPE = 1
static u8 FIXED_LIT_LEN[288];
static u8 FIXED_DIST_LEN[32];
static bool FIXED_INITIALIZED = false;

static void initFixedTables() {
    if (FIXED_INITIALIZED) return;
    for (int i = 0; i <= 143; ++i) FIXED_LIT_LEN[i] = 8;
    for (int i = 144; i <= 255; ++i) FIXED_LIT_LEN[i] = 9;
    for (int i = 256; i <= 279; ++i) FIXED_LIT_LEN[i] = 7;
    for (int i = 280; i <= 287; ++i) FIXED_LIT_LEN[i] = 8;
    for (int i = 0; i < 32; ++i) FIXED_DIST_LEN[i] = 5;
    FIXED_INITIALIZED = true;
}

// =============================================================================
// Checksum Utilities (CRC32 and Adler-32)
// =============================================================================

static u32 crc32(const u8 *buf, usz len) {
    u32 crc = 0xFFFFFFFF;
    for (usz i = 0; i < len; ++i) {
        crc ^= buf[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

static u32 adler32(const u8 *buf, usz len) {
    u32 s1 = 1;
    u32 s2 = 0;
    while (len > 0) {
        usz k = len < 5552 ? len : 5552;
        len -= k;
        while (k >= 16) {
            for (int i = 0; i < 16; ++i) {
                s1 += *buf++;
                s2 += s1;
            }
            k -= 16;
        }
        while (k > 0) {
            s1 += *buf++;
            s2 += s1;
            --k;
        }
        s1 %= 65521;
        s2 %= 65521;
    }
    return (s2 << 16) | s1;
}

// =============================================================================
// Bit Readers & Bit Writers
// =============================================================================

struct BitReader {
    const u8 *src;
    usz size;
    usz bytePos;
    u64 bitBuf;
    int bitCount;

    BitReader(const u8 *s, usz sz) : src(s), size(sz), bytePos(0), bitBuf(0), bitCount(0) {}

    inline bool ensureBits(int n) {
        while (bitCount < n) {
            if (bytePos >= size) return false;
            bitBuf |= ((u64)src[bytePos++]) << bitCount;
            bitCount += 8;
        }
        return true;
    }

    inline u32 peekBits(int n) {
        ensureBits(n);
        return (u32)(bitBuf & ((1ULL << n) - 1));
    }

    inline void dropBits(int n) {
        bitBuf >>= n;
        bitCount -= n;
        if (bitCount < 0) bitCount = 0;
    }

    inline u32 readBits(int n) {
        if (!ensureBits(n)) {
            u32 available = (bitCount > 0) ? (u32)(bitBuf & ((1ULL << bitCount) - 1)) : 0;
            dropBits(bitCount);
            return available;
        }
        u32 val = (u32)(bitBuf & ((1ULL << n) - 1));
        dropBits(n);
        return val;
    }

    inline void alignToByte() {
        int rem = bitCount % 8;
        if (rem > 0) {
            dropBits(rem);
        }
    }
};

struct BitWriter {
    String &out;
    u64 bitBuf;
    int bitCount;

    BitWriter(String &o) : out(o), bitBuf(0), bitCount(0) {}

    inline void writeBits(u32 val, int n) {
        bitBuf |= ((u64)(val & ((1ULL << n) - 1))) << bitCount;
        bitCount += n;
        while (bitCount >= 8) {
            out.push((u8)(bitBuf & 0xFF));
            bitBuf >>= 8;
            bitCount -= 8;
        }
    }

    inline void flushByte() {
        if (bitCount > 0) {
            out.push((u8)(bitBuf & 0xFF));
            bitBuf = 0;
            bitCount = 0;
        }
    }
};

// =============================================================================
// Huffman Decoder (Table-Accelerated Prefix Tree)
// =============================================================================

struct HuffmanDecoder {
    static constexpr int TABLE_BITS = 9;
    static constexpr int TABLE_SIZE = 1 << TABLE_BITS;

    // Fast direct lookup table for codes <= TABLE_BITS
    // Entry format: (symbol << 4) | length, or 0 if empty / requires tree traversal
    u16 fastTable[TABLE_SIZE];

    // Tree nodes for codes > TABLE_BITS
    struct Node {
        int child[2];
        i16 symbol;
    };

    Node tree[576];
    int nodeCount;

    HuffmanDecoder() { clear(); }

    void clear() {
        memset(fastTable, 0, sizeof(fastTable));
        memset(tree, 0, sizeof(tree));
        nodeCount = 1;
        tree[0].child[0] = -1;
        tree[0].child[1] = -1;
        tree[0].symbol = -1;
    }

    // Build canonical Huffman tree from code lengths
    bool build(const u8 *lengths, int count) {
        clear();
        int maxLen = 0;
        int blCount[16] = {0};
        for (int i = 0; i < count; ++i) {
            u8 l = lengths[i];
            if (l > 15) return false;
            if (l > 0) {
                blCount[l]++;
                if (l > maxLen) maxLen = l;
            }
        }

        if (maxLen == 0) return true; // Empty tree

        // Calculate start codes for each length (RFC 1951 section 3.2.2)
        int nextCode[16] = {0};
        int code = 0;
        for (int bits = 1; bits <= 15; ++bits) {
            code = (code + blCount[bits - 1]) << 1;
            nextCode[bits] = code;
        }

        for (int i = 0; i < count; ++i) {
            int len = lengths[i];
            if (len == 0) continue;
            int c = nextCode[len]++;

            // Reverse bits of code for LSB-first reading
            u16 revCode = 0;
            for (int b = 0; b < len; ++b) {
                revCode |= ((c >> (len - 1 - b)) & 1) << b;
            }

            if (len <= TABLE_BITS) {
                // Populate all table entries with this prefix
                u16 entry = (u16)(((u16)i << 4) | (len & 0x0F));
                int fillCount = 1 << (TABLE_BITS - len);
                for (int f = 0; f < fillCount; ++f) {
                    fastTable[revCode | (f << len)] = entry;
                }
            } else {
                // Insert into tree
                int curr = 0;
                for (int b = 0; b < len; ++b) {
                    int bit = (revCode >> b) & 1;
                    if (tree[curr].child[bit] == -1) {
                        if (nodeCount >= 576) return false; // Overflow guard
                        int next = nodeCount++;
                        tree[next].child[0] = -1;
                        tree[next].child[1] = -1;
                        tree[next].symbol = -1;
                        tree[curr].child[bit] = next;
                    }
                    curr = tree[curr].child[bit];
                }
                tree[curr].symbol = (i16)i;
            }
        }
        return true;
    }

    inline int decode(BitReader &br) const {
        if (!br.ensureBits(1)) return -1;
        u32 peek = br.peekBits(TABLE_BITS);
        u16 fast = fastTable[peek];
        if (fast != 0) {
            int len = fast & 0x0F;
            br.dropBits(len);
            return fast >> 4;
        }

        // Tree fallback for codes > TABLE_BITS
        int curr = 0;
        while (curr >= 0 && tree[curr].symbol == -1) {
            if (!br.ensureBits(1)) return -1;
            int bit = br.readBits(1);
            curr = tree[curr].child[bit];
        }
        if (curr < 0) return -1;
        return tree[curr].symbol;
    }
};

// =============================================================================
// INFLATE Implementation (RFC 1951 / 1950 / 1952)
// =============================================================================

static bool inflateRaw(BitReader &br, String &out) {
    initFixedTables();

    HuffmanDecoder litDecoder;
    HuffmanDecoder distDecoder;

    bool bfinal = false;

    while (!bfinal) {
        if (!br.ensureBits(3)) return false;
        bfinal = br.readBits(1) != 0;
        u32 btype = br.readBits(2);

        if (btype == 0) {
            // Uncompressed block (stored)
            br.alignToByte();
            if (!br.ensureBits(32)) return false;
            u16 len = (u16)br.readBits(16);
            u16 nlen = (u16)br.readBits(16);
            if ((u16)(len ^ 0xFFFF) != nlen) return false;

            for (u16 i = 0; i < len; ++i) {
                if (!br.ensureBits(8)) return false;
                out.push((u8)br.readBits(8));
            }
        } else if (btype == 1 || btype == 2) {
            // Huffman block
            if (btype == 1) {
                // Fixed Huffman
                if (!litDecoder.build(FIXED_LIT_LEN, 288)) return false;
                if (!distDecoder.build(FIXED_DIST_LEN, 32)) return false;
            } else {
                // Dynamic Huffman
                if (!br.ensureBits(14)) return false;
                u32 hlit = br.readBits(5) + 257;
                u32 hdist = br.readBits(5) + 1;
                u32 hclen = br.readBits(4) + 4;

                if (hlit > 286 || hdist > 30) return false;

                u8 clLens[19] = {0};
                for (u32 i = 0; i < hclen; ++i) {
                    if (!br.ensureBits(3)) return false;
                    clLens[CL_ORDER[i]] = (u8)br.readBits(3);
                }

                HuffmanDecoder clDecoder;
                if (!clDecoder.build(clLens, 19)) return false;

                u8 allLens[286 + 32] = {0};
                u32 totalCodes = hlit + hdist;
                u32 idx = 0;

                while (idx < totalCodes) {
                    int sym = clDecoder.decode(br);
                    if (sym < 0) return false;

                    if (sym < 16) {
                        allLens[idx++] = (u8)sym;
                    } else if (sym == 16) {
                        // Repeat previous length 3..6 times
                        if (idx == 0) return false;
                        if (!br.ensureBits(2)) return false;
                        u32 repeat = br.readBits(2) + 3;
                        if (idx + repeat > totalCodes) return false;
                        u8 prev = allLens[idx - 1];
                        for (u32 r = 0; r < repeat; ++r) allLens[idx++] = prev;
                    } else if (sym == 17) {
                        // Repeat 0 for 3..10 times
                        if (!br.ensureBits(3)) return false;
                        u32 repeat = br.readBits(3) + 3;
                        if (idx + repeat > totalCodes) return false;
                        for (u32 r = 0; r < repeat; ++r) allLens[idx++] = 0;
                    } else if (sym == 18) {
                        // Repeat 0 for 11..138 times
                        if (!br.ensureBits(7)) return false;
                        u32 repeat = br.readBits(7) + 11;
                        if (idx + repeat > totalCodes) return false;
                        for (u32 r = 0; r < repeat; ++r) allLens[idx++] = 0;
                    } else {
                        return false;
                    }
                }

                if (!litDecoder.build(allLens, (int)hlit)) return false;
                if (!distDecoder.build(allLens + hlit, (int)hdist)) return false;
            }

            // Decode block stream
            while (true) {
                int sym = litDecoder.decode(br);
                if (sym < 0) return false;
                if (sym < 256) {
                    out.push((u8)sym);
                } else if (sym == 256) {
                    // End of block
                    break;
                } else if (sym <= 285) {
                    // Length + distance match
                    int lenIdx = sym - 257;
                    u32 length = LENGTH_BASE[lenIdx];
                    int extraLen = LENGTH_EXTRA[lenIdx];
                    if (extraLen > 0) {
                        if (!br.ensureBits(extraLen)) return false;
                        length += br.readBits(extraLen);
                    }

                    int distSym = distDecoder.decode(br);
                    if (distSym < 0 || distSym > 29) return false;
                    u32 distance = DISTANCE_BASE[distSym];
                    int extraDist = DISTANCE_EXTRA[distSym];
                    if (extraDist > 0) {
                        if (!br.ensureBits(extraDist)) return false;
                        distance += br.readBits(extraDist);
                    }

                    if (distance > out.size() || distance == 0) {
                        return false; // Copy outside valid window
                    }

                    usz srcIdx = out.size() - distance;
                    for (u32 i = 0; i < length; ++i) {
                        out.push(out[srcIdx + i]);
                    }
                } else {
                    return false;
                }
            }
        } else {
            // Invalid block type (3)
            return false;
        }
    }

    return true;
}

String inflate(const String &input) {
    if (input.isEmpty()) return String();

    const u8 *data = (const u8*)input.data();
    usz size = input.size();

    usz startOffset = 0;
    usz endOffset = size;

    // 1. Detect Gzip Container (RFC 1952)
    if (size >= 10 && data[0] == 0x1F && data[1] == 0x8B && data[2] == 0x08) {
        u8 flg = data[3];
        startOffset = 10;

        if (flg & 0x04) {
            // FEXTRA
            if (startOffset + 2 > size) return String();
            u16 xlen = (u16)data[startOffset] | ((u16)data[startOffset + 1] << 8);
            startOffset += 2 + xlen;
        }
        if (flg & 0x08) {
            // FNAME: zero-terminated string
            while (startOffset < size && data[startOffset] != 0) startOffset++;
            startOffset++; // skip null byte
        }
        if (flg & 0x10) {
            // FCOMMENT: zero-terminated string
            while (startOffset < size && data[startOffset] != 0) startOffset++;
            startOffset++; // skip null byte
        }
        if (flg & 0x02) {
            // FHCRC: 2-byte header crc
            startOffset += 2;
        }
        if (startOffset > size || size < 8) return String();
        endOffset = size - 8; // Exclude 4-byte CRC32 and 4-byte ISIZE footer
    }
    // 2. Detect Zlib Container (RFC 1950)
    else if (size >= 6) {
        u8 cmf = data[0];
        u8 flg = data[1];
        if ((cmf & 0x0F) == 8 && ((cmf * 256 + flg) % 31 == 0)) {
            startOffset = 2;
            if (flg & 0x20) {
                // FDICT: 4-byte dictionary ID
                startOffset += 4;
            }
            if (startOffset <= size - 4) {
                endOffset = size - 4; // Exclude 4-byte Adler-32
            }
        }
    }

    if (startOffset >= endOffset) return String();

    String decompressed;
    BitReader br(data + startOffset, endOffset - startOffset);
    if (!inflateRaw(br, decompressed)) {
        // Fallback: try whole buffer as raw DEFLATE if header probe failed
        if (startOffset != 0) {
            String fallback;
            BitReader brRaw(data, size);
            if (inflateRaw(brRaw, fallback)) {
                return fallback;
            }
        }
        return String();
    }

    return decompressed;
}

// =============================================================================
// DEFLATE Implementation with Dynamic Huffman & LZ77 Match Finder
// =============================================================================

// Canonical Huffman tree builder with length limits
static void buildHuffmanLengths(const u32 *freqIn, int count, u8 *outLens, int maxBitLen) {
    memset(outLens, 0, count);

    struct HNode {
        u32 weight;
        int left;
        int right;
        int symbol;
    };

    static constexpr int MAX_SYMS = 320;
    if (count > MAX_SYMS) count = MAX_SYMS;

    u32 freq[MAX_SYMS];
    for (int i = 0; i < count; ++i) freq[i] = freqIn[i];

    HNode nodes[MAX_SYMS * 2 + 1];
    int active[MAX_SYMS + 1];

    while (true) {
        int activeCount = 0;
        int nodeCount = 0;

        for (int i = 0; i < count; ++i) {
            if (freq[i] > 0) {
                nodes[nodeCount].weight = freq[i];
                nodes[nodeCount].left = -1;
                nodes[nodeCount].right = -1;
                nodes[nodeCount].symbol = i;
                active[activeCount++] = nodeCount++;
            }
        }

        if (activeCount == 0) return;
        if (activeCount == 1) {
            outLens[nodes[active[0]].symbol] = 1;
            return;
        }

        while (activeCount > 1) {
            int m1 = 0, m2 = 1;
            if (nodes[active[m1]].weight > nodes[active[m2]].weight) {
                int t = m1; m1 = m2; m2 = t;
            }
            for (int i = 2; i < activeCount; ++i) {
                if (nodes[active[i]].weight < nodes[active[m1]].weight) {
                    m2 = m1;
                    m1 = i;
                } else if (nodes[active[i]].weight < nodes[active[m2]].weight) {
                    m2 = i;
                }
            }

            int n1 = active[m1];
            int n2 = active[m2];

            int parent = nodeCount++;
            nodes[parent].weight = nodes[n1].weight + nodes[n2].weight;
            nodes[parent].left = n1;
            nodes[parent].right = n2;
            nodes[parent].symbol = -1;

            active[m1] = parent;
            active[m2] = active[activeCount - 1];
            activeCount--;
        }

        int maxDepth = 0;
        auto computeLengths = [&](auto &self, int nodeIdx, int depth) -> void {
            if (nodeIdx < 0) return;
            if (nodes[nodeIdx].symbol >= 0) {
                outLens[nodes[nodeIdx].symbol] = (u8)depth;
                if (depth > maxDepth) maxDepth = depth;
                return;
            }
            self(self, nodes[nodeIdx].left, depth + 1);
            self(self, nodes[nodeIdx].right, depth + 1);
        };

        computeLengths(computeLengths, active[0], 0);

        if (maxDepth <= maxBitLen) {
            break;
        }

        for (int i = 0; i < count; ++i) {
            if (freq[i] > 1) {
                freq[i] = (freq[i] + 1) >> 1;
            }
        }
    }
}

// Generate canonical codes from lengths
static void generateCodes(const u8 *lens, int count, u16 *outCodes) {
    int blCount[16] = {0};
    for (int i = 0; i < count; ++i) {
        if (lens[i] > 0 && lens[i] <= 15) blCount[lens[i]]++;
    }

    int nextCode[16] = {0};
    int code = 0;
    for (int bits = 1; bits <= 15; ++bits) {
        code = (code + blCount[bits - 1]) << 1;
        nextCode[bits] = code;
    }

    for (int i = 0; i < count; ++i) {
        int l = lens[i];
        if (l > 0) {
            int c = nextCode[l]++;
            u16 rev = 0;
            for (int b = 0; b < l; ++b) {
                rev |= ((c >> (l - 1 - b)) & 1) << b;
            }
            outCodes[i] = rev;
        } else {
            outCodes[i] = 0;
        }
    }
}

// Map length 3..258 to symbol 257..285 and extra bits
static inline void getLengthCode(u32 len, u32 &code, u32 &extraBits, int &extraCount) {
    for (int i = 28; i >= 0; --i) {
        if (len >= LENGTH_BASE[i]) {
            code = 257 + i;
            extraCount = LENGTH_EXTRA[i];
            extraBits = len - LENGTH_BASE[i];
            return;
        }
    }
    code = 257; extraBits = 0; extraCount = 0;
}

// Map distance 1..32768 to symbol 0..29 and extra bits
static inline void getDistanceCode(u32 dist, u32 &code, u32 &extraBits, int &extraCount) {
    for (int i = 29; i >= 0; --i) {
        if (dist >= DISTANCE_BASE[i]) {
            code = i;
            extraCount = DISTANCE_EXTRA[i];
            extraBits = dist - DISTANCE_BASE[i];
            return;
        }
    }
    code = 0; extraBits = 0; extraCount = 0;
}

String deflate(const String &input, i32 level) {
    if (input.isEmpty()) {
        // Empty DEFLATE stream: 1-bit BFINAL=1, 2-bit BTYPE=01 (fixed), symbol 256 (0000000)
        String out;
        BitWriter bw(out);
        bw.writeBits(1, 1); // bfinal = 1
        bw.writeBits(1, 2); // btype = 01 (fixed)
        bw.writeBits(0, 7); // code for symbol 256 in fixed huffman is 7 zero bits
        bw.flushByte();
        return out;
    }

    if (level < 0) level = 6;
    if (level > 9) level = 9;

    const u8 *src = (const u8*)input.data();
    usz srcLen = input.size();

    // Level 0: Uncompressed stored blocks
    if (level == 0) {
        String out;
        BitWriter bw(out);
        usz pos = 0;
        while (pos < srcLen) {
            usz chunkSize = srcLen - pos;
            if (chunkSize > 65535) chunkSize = 65535;
            bool isLast = (pos + chunkSize == srcLen);
            bw.writeBits(isLast ? 1 : 0, 1);
            bw.writeBits(0, 2); // BTYPE = 00
            bw.flushByte();

            u16 len = (u16)chunkSize;
            u16 nlen = ~len;
            out.push((u8)(len & 0xFF));
            out.push((u8)((len >> 8) & 0xFF));
            out.push((u8)(nlen & 0xFF));
            out.push((u8)((nlen >> 8) & 0xFF));
            out.append((const char*)(src + pos), chunkSize);
            pos += chunkSize;
        }
        return out;
    }

    // LZ77 Tokenizer
    struct LZToken {
        u16 litOrLen;  // literal byte (0..255), or length (3..258)
        u16 dist;      // 0 for literal, >0 for distance
    };

    Array<LZToken> tokens;

    // Hash table for 3-byte prefix matching (8192 buckets)
    static constexpr int HASH_SIZE = 8192;
    static constexpr int HASH_MASK = HASH_SIZE - 1;
    static constexpr int WINDOW_SIZE = 32768;
    static constexpr int WINDOW_MASK = WINDOW_SIZE - 1;

    i32 head[HASH_SIZE];
    i32 prev[WINDOW_SIZE];
    for (int i = 0; i < HASH_SIZE; ++i) head[i] = -1;
    for (int i = 0; i < WINDOW_SIZE; ++i) prev[i] = -1;

    auto hash3 = [](const u8 *p) -> u32 {
        return ((u32)p[0] ^ ((u32)p[1] << 4) ^ ((u32)p[2] << 8)) & HASH_MASK;
    };

    int maxChain = (level <= 3) ? 8 : (level <= 6 ? 32 : 128);

    usz pos = 0;
    while (pos < srcLen) {
        u32 bestLen = 0;
        u32 bestDist = 0;

        if (pos + 3 <= srcLen) {
            u32 h = hash3(src + pos);
            int matchPos = head[h];
            prev[pos & WINDOW_MASK] = matchPos;
            head[h] = (i32)pos;

            int chain = 0;
            while (matchPos >= 0 && chain++ < maxChain) {
                if ((pos - matchPos) > WINDOW_SIZE) break;
                u32 dist = (u32)(pos - matchPos);

                // Quick compare
                if (src[matchPos + bestLen] == src[pos + bestLen] &&
                    src[matchPos] == src[pos]) {
                    u32 matchLen = 0;
                    while (pos + matchLen < srcLen && matchLen < 258 &&
                           src[matchPos + matchLen] == src[pos + matchLen]) {
                        matchLen++;
                    }
                    if (matchLen > bestLen && matchLen >= 3) {
                        bestLen = matchLen;
                        bestDist = dist;
                        if (bestLen >= 258) break;
                    }
                }
                matchPos = prev[matchPos & WINDOW_MASK];
            }
        }

        if (bestLen >= 3) {
            tokens.push({ (u16)bestLen, (u16)bestDist });
            // Update hash table for skipped bytes
            for (u32 k = 1; k < bestLen; ++k) {
                if (pos + k + 3 <= srcLen) {
                    u32 h2 = hash3(src + pos + k);
                    prev[(pos + k) & WINDOW_MASK] = head[h2];
                    head[h2] = (i32)(pos + k);
                }
            }
            pos += bestLen;
        } else {
            tokens.push({ (u16)src[pos], 0 });
            pos++;
        }
    }

    // Collect frequencies for Dynamic Huffman
    u32 litFreq[286] = {0};
    u32 distFreq[30] = {0};

    litFreq[256] = 1; // End of block symbol

    for (usz i = 0; i < tokens.size(); ++i) {
        const LZToken &tok = tokens[i];
        if (tok.dist == 0) {
            litFreq[tok.litOrLen]++;
        } else {
            u32 code, eb; int ec;
            getLengthCode(tok.litOrLen, code, eb, ec);
            litFreq[code]++;

            u32 dcode, deb; int dec;
            getDistanceCode(tok.dist, dcode, deb, dec);
            distFreq[dcode]++;
        }
    }

    // Generate Huffman code lengths
    u8 litLens[286];
    u8 distLens[30];
    buildHuffmanLengths(litFreq, 286, litLens, 15);
    buildHuffmanLengths(distFreq, 30, distLens, 15);

    // Determine HLIT (must be at least 257) and HDIST (at least 1)
    int hlit = 286;
    while (hlit > 257 && litLens[hlit - 1] == 0) hlit--;

    int hdist = 30;
    while (hdist > 1 && distLens[hdist - 1] == 0) hdist--;

    // Encode code lengths using RLE symbols
    u8 rleSyms[316 * 2];
    u8 rleExtra[316 * 2];
    u8 rleExtraBits[316 * 2];
    int rleCount = 0;

    u8 combinedLens[316];
    memcpy(combinedLens, litLens, hlit);
    memcpy(combinedLens + hlit, distLens, hdist);
    int totalCombined = hlit + hdist;

    u32 clFreq[19] = {0};

    int i = 0;
    while (i < totalCombined) {
        u8 len = combinedLens[i];
        int run = 1;
        while (i + run < totalCombined && combinedLens[i + run] == len) {
            run++;
        }
        int totalRun = run;

        if (len == 0) {
            while (run >= 11) {
                int count = run > 138 ? 138 : run;
                rleSyms[rleCount] = 18;
                rleExtra[rleCount] = (u8)(count - 11);
                rleExtraBits[rleCount] = 7;
                clFreq[18]++;
                rleCount++;
                run -= count;
            }
            if (run >= 3) {
                rleSyms[rleCount] = 17;
                rleExtra[rleCount] = (u8)(run - 3);
                rleExtraBits[rleCount] = 3;
                clFreq[17]++;
                rleCount++;
                run = 0;
            }
            while (run > 0) {
                rleSyms[rleCount] = 0;
                rleExtra[rleCount] = 0;
                rleExtraBits[rleCount] = 0;
                clFreq[0]++;
                rleCount++;
                run--;
            }
        } else {
            rleSyms[rleCount] = len;
            rleExtra[rleCount] = 0;
            rleExtraBits[rleCount] = 0;
            clFreq[len]++;
            rleCount++;
            run--;

            while (run >= 3) {
                int count = run > 6 ? 6 : run;
                rleSyms[rleCount] = 16;
                rleExtra[rleCount] = (u8)(count - 3);
                rleExtraBits[rleCount] = 2;
                clFreq[16]++;
                rleCount++;
                run -= count;
            }
            while (run > 0) {
                rleSyms[rleCount] = len;
                rleExtra[rleCount] = 0;
                rleExtraBits[rleCount] = 0;
                clFreq[len]++;
                rleCount++;
                run--;
            }
        }
        i += totalRun;
    }

    // Build code length Huffman tree
    u8 clLens[19];
    buildHuffmanLengths(clFreq, 19, clLens, 7);

    int hclen = 19;
    while (hclen > 4 && clLens[CL_ORDER[hclen - 1]] == 0) hclen--;

    // Generate canonical bit codes
    u16 litCodes[286];
    u16 distCodes[30];
    u16 clCodes[19];
    generateCodes(litLens, 286, litCodes);
    generateCodes(distLens, 30, distCodes);
    generateCodes(clLens, 19, clCodes);

    // Emit Dynamic Huffman stream
    String out;
    BitWriter bw(out);

    bw.writeBits(1, 1); // BFINAL = 1
    bw.writeBits(2, 2); // BTYPE = 10 (Dynamic Huffman)

    bw.writeBits((u32)(hlit - 257), 5);
    bw.writeBits((u32)(hdist - 1), 5);
    bw.writeBits((u32)(hclen - 4), 4);

    for (int k = 0; k < hclen; ++k) {
        bw.writeBits(clLens[CL_ORDER[k]], 3);
    }

    // Write encoded code lengths
    for (int k = 0; k < rleCount; ++k) {
        u8 sym = rleSyms[k];
        bw.writeBits(clCodes[sym], clLens[sym]);
        if (rleExtraBits[k] > 0) {
            bw.writeBits(rleExtra[k], rleExtraBits[k]);
        }
    }

    // Write compressed tokens
    for (usz k = 0; k < tokens.size(); ++k) {
        const LZToken &tok = tokens[k];
        if (tok.dist == 0) {
            bw.writeBits(litCodes[tok.litOrLen], litLens[tok.litOrLen]);
        } else {
            u32 code, eb; int ec;
            getLengthCode(tok.litOrLen, code, eb, ec);
            bw.writeBits(litCodes[code], litLens[code]);
            if (ec > 0) bw.writeBits(eb, ec);

            u32 dcode, deb; int dec;
            getDistanceCode(tok.dist, dcode, deb, dec);
            bw.writeBits(distCodes[dcode], distLens[dcode]);
            if (dec > 0) bw.writeBits(deb, dec);
        }
    }

    // Write End of Block symbol (256)
    bw.writeBits(litCodes[256], litLens[256]);
    bw.flushByte();

    // If compressed output is somehow larger than raw uncompressed block, fallback to stored
    if (out.size() >= srcLen + 5) {
        String fallback;
        BitWriter fbw(fallback);
        fbw.writeBits(1, 1);
        fbw.writeBits(0, 2);
        fbw.flushByte();
        u16 len = (u16)srcLen;
        u16 nlen = ~len;
        fallback.push((u8)(len & 0xFF));
        fallback.push((u8)((len >> 8) & 0xFF));
        fallback.push((u8)(nlen & 0xFF));
        fallback.push((u8)((nlen >> 8) & 0xFF));
        fallback.append((const char*)src, srcLen);
        return fallback;
    }

    return out;
}

} // namespace Algorithm
} // namespace Ksee
