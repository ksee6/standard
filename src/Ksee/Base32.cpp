/**
 * @file Base32.cpp
 * @brief Base32 (RFC 4648) and Crockford's Base32 implementation.
 */

#include <Ksee/Base32.hpp>

namespace Ksee {

// -------------------------------------------------------------------------
// Crockford's Base32
// -------------------------------------------------------------------------

static const char CB32_CHARS[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static inline int cb32CharVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c == 'I' || c == 'L') return 1;
    if (c == 'O') return 0;
    if (c >= 'A' && c <= 'H') return c - 'A' + 10;
    if (c >= 'J' && c <= 'K') return c - 'J' + 18;
    if (c >= 'M' && c <= 'N') return c - 'M' + 20;
    if (c >= 'P' && c <= 'T') return c - 'P' + 22;
    if (c >= 'V' && c <= 'Z') return c - 'V' + 27;
    return -1;
}

String encodeCB32BigEndian(const u8 *ptr, usz length) {
    if (!ptr || length == 0) return String();

    usz totalBits = length * 8;
    usz numChars = (totalBits + 4) / 5;
    usz padBits = (numChars * 5) - totalBits;

    String out;
    for (usz c = 0; c < numChars; c++) {
        u32 val = 0;
        for (int i = 0; i < 5; i++) {
            usz bitIdx = c * 5 + i;
            u32 bit = 0;
            if (bitIdx >= padBits) {
                usz realBit = bitIdx - padBits;
                usz byteIdx = realBit / 8;
                usz bitInByte = 7 - (realBit % 8);
                bit = (ptr[byteIdx] >> bitInByte) & 1;
            }
            val = (val << 1) | bit;
        }
        out += CB32_CHARS[val & 31];
    }
    return out;
}

usz decodeCB32BigEndian(const String &str, u8 *out, usz maxLength) {
    if (!out || maxLength == 0 || str.size() == 0) return 0;

    Array<u8> symbols;
    for (usz i = 0; i < str.size(); i++) {
        char c = str[i];
        if (c == '-' || c == ' ') continue;
        int v = cb32CharVal(c);
        if (v >= 0) symbols.push((u8)v);
    }
    if (symbols.size() == 0) return 0;

    usz streamBits = symbols.size() * 5;
    usz targetBytes = streamBits / 8;
    if (targetBytes > maxLength) targetBytes = maxLength;
    if (targetBytes == 0) return 0;

    usz targetBits = targetBytes * 8;
    usz skipBits = streamBits - targetBits;

    for (usz i = 0; i < targetBytes; i++) out[i] = 0;

    for (usz b = 0; b < targetBits; b++) {
        usz streamIdx = skipBits + b;
        usz symIdx = streamIdx / 5;
        usz bitInSym = 4 - (streamIdx % 5);
        u8 bit = (symbols[symIdx] >> bitInSym) & 1;
        out[b / 8] |= (bit << (7 - (b % 8)));
    }
    return targetBytes;
}

String encodeCB32(u32 v) {
    u8 bytes[4];
    for (int i = 0; i < 4; i++) {
        bytes[i] = (u8)((v >> ((3 - i) * 8)) & 0xFF);
    }
    return encodeCB32BigEndian(bytes, 4);
}

String encodeCB32(u64 v) {
    u8 bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (u8)((v >> ((7 - i) * 8)) & 0xFF);
    }
    return encodeCB32BigEndian(bytes, 8);
}

String encodeCB32(const u128 &v) {
    return encodeCB32BigEndian(v.data, 16);
}

String encodeCB32(const u256 &v) {
    return encodeCB32BigEndian(v.data, 32);
}

String encodeCB32(const u512 &v) {
    return encodeCB32BigEndian(v.data, 64);
}

String encodeCB32(const String &str) {
    if (str.size() == 0) return String();
    const u8 *data = (const u8 *)str.data();
    usz len = str.size();
    String res;
    usz i = 0;
    while (i < len) {
        usz rem = len - i;
        u8 b0 = data[i];
        u8 b1 = rem > 1 ? data[i + 1] : 0;
        u8 b2 = rem > 2 ? data[i + 2] : 0;
        u8 b3 = rem > 3 ? data[i + 3] : 0;
        u8 b4 = rem > 4 ? data[i + 4] : 0;

        res += CB32_CHARS[(b0 >> 3) & 0x1F];
        res += CB32_CHARS[((b0 & 0x07) << 2) | ((b1 >> 6) & 0x03)];
        if (rem > 1) {
            res += CB32_CHARS[(b1 >> 1) & 0x1F];
            res += CB32_CHARS[((b1 & 0x01) << 4) | ((b2 >> 4) & 0x0F)];
        } else break;
        if (rem > 2) {
            res += CB32_CHARS[((b2 & 0x0F) << 1) | ((b3 >> 7) & 0x01)];
        } else break;
        if (rem > 3) {
            res += CB32_CHARS[(b3 >> 2) & 0x1F];
            res += CB32_CHARS[((b3 & 0x03) << 3) | ((b4 >> 5) & 0x07)];
        } else break;
        if (rem > 4) {
            res += CB32_CHARS[b4 & 0x1F];
        } else break;
        i += 5;
    }
    return res;
}

bool decodeCB32(const String &str, u32 &out) {
    u8 bytes[4] = {0};
    usz n = decodeCB32BigEndian(str, bytes, 4);
    if (n == 0 && str.size() > 0) return false;
    out = 0;
    for (usz i = 0; i < n; i++) {
        out = (out << 8) | bytes[i];
    }
    return true;
}

bool decodeCB32(const String &str, u64 &out) {
    u8 bytes[8] = {0};
    usz n = decodeCB32BigEndian(str, bytes, 8);
    if (n == 0 && str.size() > 0) return false;
    out = 0;
    for (usz i = 0; i < n; i++) {
        out = (out << 8) | bytes[i];
    }
    return true;
}

bool decodeCB32(const String &str, u128 &out) {
    for (usz i = 0; i < 16; ++i) out.data[i] = 0;
    usz n = decodeCB32BigEndian(str, out.data, 16);
    return n == 16 || (n > 0 && str.size() <= 26);
}

bool decodeCB32(const String &str, u256 &out) {
    for (usz i = 0; i < 32; ++i) out.data[i] = 0;
    usz n = decodeCB32BigEndian(str, out.data, 32);
    return n == 32 || (n > 0 && str.size() <= 52);
}

bool decodeCB32(const String &str, u512 &out) {
    for (usz i = 0; i < 64; ++i) out.data[i] = 0;
    usz n = decodeCB32BigEndian(str, out.data, 64);
    return n == 64 || (n > 0 && str.size() <= 103);
}

bool decodeCB32(const String &str, String &out) {
    out = String();
    Array<u8> symbols;
    for (usz i = 0; i < str.size(); i++) {
        char c = str[i];
        if (c == '=' || c == '-' || c == ' ') continue;
        int v = cb32CharVal(c);
        if (v < 0) return false;
        symbols.push((u8)v);
    }
    if (symbols.size() == 0) return true;

    usz totalBits = symbols.size() * 5;
    usz totalBytes = totalBits / 8;
    for (usz b = 0; b < totalBytes; b++) {
        u8 byteVal = 0;
        for (int bit = 0; bit < 8; bit++) {
            usz bitIdx = b * 8 + bit;
            usz symIdx = bitIdx / 5;
            usz bitInSym = 4 - (bitIdx % 5);
            u8 bitVal = (symbols[symIdx] >> bitInSym) & 1;
            byteVal |= (bitVal << (7 - bit));
        }
        out.push(byteVal);
    }
    return true;
}

u32 CB32Decoder::toU32() const {
    u32 v = 0;
    decodeCB32(str, v);
    return v;
}

u64 CB32Decoder::toU64() const {
    u64 v = 0;
    decodeCB32(str, v);
    return v;
}

u128 CB32Decoder::toU128() const {
    u128 v;
    decodeCB32(str, v);
    return v;
}

u256 CB32Decoder::toU256() const {
    u256 v;
    decodeCB32(str, v);
    return v;
}

u512 CB32Decoder::toU512() const {
    u512 v;
    decodeCB32(str, v);
    return v;
}

String CB32Decoder::toString() const {
    String out;
    decodeCB32(str, out);
    return out;
}

CB32Decoder::operator u32() const { return toU32(); }
CB32Decoder::operator u64() const { return toU64(); }
CB32Decoder::operator u128() const { return toU128(); }
CB32Decoder::operator u256() const { return toU256(); }
CB32Decoder::operator u512() const { return toU512(); }
CB32Decoder::operator String() const { return toString(); }

CB32Decoder decodeCB32(const String &str) {
    return CB32Decoder{str};
}

// -------------------------------------------------------------------------
// Standard RFC 4648 Base32
// -------------------------------------------------------------------------

static const char B32_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static inline int b32CharVal(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    return -1;
}

String encodeB32BigEndian(const u8 *ptr, usz length) {
    if (!ptr || length == 0) return String();

    usz totalBits = length * 8;
    usz numChars = (totalBits + 4) / 5;
    usz padBits = (numChars * 5) - totalBits;

    String out;
    for (usz c = 0; c < numChars; c++) {
        u32 val = 0;
        for (int i = 0; i < 5; i++) {
            usz bitIdx = c * 5 + i;
            u32 bit = 0;
            if (bitIdx >= padBits) {
                usz realBit = bitIdx - padBits;
                usz byteIdx = realBit / 8;
                usz bitInByte = 7 - (realBit % 8);
                bit = (ptr[byteIdx] >> bitInByte) & 1;
            }
            val = (val << 1) | bit;
        }
        out += B32_CHARS[val & 31];
    }
    return out;
}

usz decodeB32BigEndian(const String &str, u8 *out, usz maxLength) {
    if (!out || maxLength == 0 || str.size() == 0) return 0;

    Array<u8> symbols;
    for (usz i = 0; i < str.size(); i++) {
        char c = str[i];
        if (c == '=' || c == '-' || c == ' ') continue;
        int v = b32CharVal(c);
        if (v >= 0) symbols.push((u8)v);
    }
    if (symbols.size() == 0) return 0;

    usz streamBits = symbols.size() * 5;
    usz targetBytes = streamBits / 8;
    if (targetBytes > maxLength) targetBytes = maxLength;
    if (targetBytes == 0) return 0;

    usz targetBits = targetBytes * 8;
    usz skipBits = streamBits - targetBits;

    for (usz i = 0; i < targetBytes; i++) out[i] = 0;

    for (usz b = 0; b < targetBits; b++) {
        usz streamIdx = skipBits + b;
        usz symIdx = streamIdx / 5;
        usz bitInSym = 4 - (streamIdx % 5);
        u8 bit = (symbols[symIdx] >> bitInSym) & 1;
        out[b / 8] |= (bit << (7 - (b % 8)));
    }
    return targetBytes;
}

String encodeB32(u32 v) {
    u8 bytes[4];
    for (int i = 0; i < 4; i++) {
        bytes[i] = (u8)((v >> ((3 - i) * 8)) & 0xFF);
    }
    return encodeB32BigEndian(bytes, 4);
}

String encodeB32(u64 v) {
    u8 bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (u8)((v >> ((7 - i) * 8)) & 0xFF);
    }
    return encodeB32BigEndian(bytes, 8);
}

String encodeB32(const u128 &v) {
    return encodeB32BigEndian(v.data, 16);
}

String encodeB32(const u256 &v) {
    return encodeB32BigEndian(v.data, 32);
}

String encodeB32(const u512 &v) {
    return encodeB32BigEndian(v.data, 64);
}

String encodeB32(const String &str) {
    if (str.size() == 0) return String();
    const u8 *data = (const u8 *)str.data();
    usz len = str.size();
    String res;
    usz i = 0;
    while (i < len) {
        usz rem = len - i;
        u8 b0 = data[i];
        u8 b1 = rem > 1 ? data[i + 1] : 0;
        u8 b2 = rem > 2 ? data[i + 2] : 0;
        u8 b3 = rem > 3 ? data[i + 3] : 0;
        u8 b4 = rem > 4 ? data[i + 4] : 0;

        res += B32_CHARS[(b0 >> 3) & 0x1F];
        res += B32_CHARS[((b0 & 0x07) << 2) | ((b1 >> 6) & 0x03)];
        if (rem > 1) {
            res += B32_CHARS[(b1 >> 1) & 0x1F];
            res += B32_CHARS[((b1 & 0x01) << 4) | ((b2 >> 4) & 0x0F)];
        } else {
            res += "======";
            break;
        }
        if (rem > 2) {
            res += B32_CHARS[((b2 & 0x0F) << 1) | ((b3 >> 7) & 0x01)];
        } else {
            res += "====";
            break;
        }
        if (rem > 3) {
            res += B32_CHARS[(b3 >> 2) & 0x1F];
            res += B32_CHARS[((b3 & 0x03) << 3) | ((b4 >> 5) & 0x07)];
        } else {
            res += "===";
            break;
        }
        if (rem > 4) {
            res += B32_CHARS[b4 & 0x1F];
        } else {
            res += "=";
            break;
        }
        i += 5;
    }
    return res;
}

bool decodeB32(const String &str, u32 &out) {
    u8 bytes[4] = {0};
    usz n = decodeB32BigEndian(str, bytes, 4);
    if (n == 0 && str.size() > 0) return false;
    out = 0;
    for (usz i = 0; i < n; i++) {
        out = (out << 8) | bytes[i];
    }
    return true;
}

bool decodeB32(const String &str, u64 &out) {
    u8 bytes[8] = {0};
    usz n = decodeB32BigEndian(str, bytes, 8);
    if (n == 0 && str.size() > 0) return false;
    out = 0;
    for (usz i = 0; i < n; i++) {
        out = (out << 8) | bytes[i];
    }
    return true;
}

bool decodeB32(const String &str, u128 &out) {
    for (usz i = 0; i < 16; ++i) out.data[i] = 0;
    usz n = decodeB32BigEndian(str, out.data, 16);
    return n == 16 || (n > 0 && str.size() <= 26);
}

bool decodeB32(const String &str, u256 &out) {
    for (usz i = 0; i < 32; ++i) out.data[i] = 0;
    usz n = decodeB32BigEndian(str, out.data, 32);
    return n == 32 || (n > 0 && str.size() <= 52);
}

bool decodeB32(const String &str, u512 &out) {
    for (usz i = 0; i < 64; ++i) out.data[i] = 0;
    usz n = decodeB32BigEndian(str, out.data, 64);
    return n == 64 || (n > 0 && str.size() <= 103);
}

bool decodeB32(const String &str, String &out) {
    out = String();
    Array<u8> symbols;
    for (usz i = 0; i < str.size(); i++) {
        char c = str[i];
        if (c == '=' || c == '-' || c == ' ') continue;
        int v = b32CharVal(c);
        if (v < 0) return false;
        symbols.push((u8)v);
    }
    if (symbols.size() == 0) return true;

    usz totalBits = symbols.size() * 5;
    usz totalBytes = totalBits / 8;
    for (usz b = 0; b < totalBytes; b++) {
        u8 byteVal = 0;
        for (int bit = 0; bit < 8; bit++) {
            usz bitIdx = b * 8 + bit;
            usz symIdx = bitIdx / 5;
            usz bitInSym = 4 - (bitIdx % 5);
            u8 bitVal = (symbols[symIdx] >> bitInSym) & 1;
            byteVal |= (bitVal << (7 - bit));
        }
        out.push(byteVal);
    }
    return true;
}

u32 B32Decoder::toU32() const {
    u32 v = 0;
    decodeB32(str, v);
    return v;
}

u64 B32Decoder::toU64() const {
    u64 v = 0;
    decodeB32(str, v);
    return v;
}

u128 B32Decoder::toU128() const {
    u128 v;
    decodeB32(str, v);
    return v;
}

u256 B32Decoder::toU256() const {
    u256 v;
    decodeB32(str, v);
    return v;
}

u512 B32Decoder::toU512() const {
    u512 v;
    decodeB32(str, v);
    return v;
}

String B32Decoder::toString() const {
    String out;
    decodeB32(str, out);
    return out;
}

B32Decoder::operator u32() const { return toU32(); }
B32Decoder::operator u64() const { return toU64(); }
B32Decoder::operator u128() const { return toU128(); }
B32Decoder::operator u256() const { return toU256(); }
B32Decoder::operator u512() const { return toU512(); }
B32Decoder::operator String() const { return toString(); }

B32Decoder decodeB32(const String &str) {
    return B32Decoder{str};
}

} // namespace Ksee
