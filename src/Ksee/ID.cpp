/**
 * @file ID.cpp
 * @brief UUID (v7 and v4) implementation for Ksee.
 */

#include "../../include/Ksee/ID.hpp"
#include "../../include/Ksee/Time.hpp"
#include <stdio.h>

namespace Ksee {

UUID UUID::v7() {
    u64 ms = (u64)epochMillis();

    u8 randBuf[10];
    randomFill(randBuf, 10);

    UUID u;
    u.high = (ms & 0x0000FFFFFFFFFFFFULL) << 16;
    u.high |= 0x0000000000007000ULL;
    u16 rand_a = ((u16)randBuf[0] << 8) | randBuf[1];
    u.high |= (rand_a & 0x0FFF);

    u.low = 0;
    for (int i = 2; i < 10; i++) u.low = (u.low << 8) | randBuf[i];
    u.low = (u.low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
    return u;
}

UUID UUID::v4() {
    u8 randBuf[16];
    randomFill(randBuf, 16);

    UUID u;
    u.high = 0;
    u.low = 0;
    for (int i = 0; i < 8; i++) u.high = (u.high << 8) | randBuf[i];
    for (int i = 8; i < 16; i++) u.low = (u.low << 8) | randBuf[i];

    u.high = (u.high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    u.low = (u.low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
    return u;
}

UUID::UUID() : high(0), low(0) {}

static inline int parseHexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

UUID::UUID(const String &str) {
    String s = str.trim();
    if (s.startsWith("urn:uuid:") || s.startsWith("URN:UUID:")) {
        s = s.substring(9).trim();
    }
    if (s.startsWith("{") && s.endsWith("}")) {
        s = s.substring(1, s.length() - 1).trim();
    }

    low = 0;
    high = 0;

    if (s.length() == 36) {
        if (s.charAt(8) != '-' || s.charAt(13) != '-' ||
            s.charAt(18) != '-' || s.charAt(23) != '-') {
            return;
        }
        u64 h = 0;
        u64 l = 0;
        int hexCount = 0;
        for (usz i = 0; i < 36; i++) {
            if (i == 8 || i == 13 || i == 18 || i == 23) continue;
            int n = parseHexNibble(s.charAt(i));
            if (n < 0) return;
            if (hexCount < 16) {
                h = (h << 4) | (u64)n;
            } else {
                l = (l << 4) | (u64)n;
            }
            hexCount++;
        }
        high = h;
        low = l;
    } else if (s.length() == 32) {
        u64 h = 0;
        u64 l = 0;
        for (usz i = 0; i < 32; i++) {
            int n = parseHexNibble(s.charAt(i));
            if (n < 0) return;
            if (i < 16) {
                h = (h << 4) | (u64)n;
            } else {
                l = (l << 4) | (u64)n;
            }
        }
        high = h;
        low = l;
    }
}

UUID::UUID(const char *string) : UUID(string ? String(string) : String()) {}

UUID::UUID(u64 high, u64 low) : high(high), low(low) {}

String UUID::toString() const {
    char buf[37];
    snprintf(buf, sizeof(buf),
        "%08x-%04x-%04x-%04x-%012llx",
        (unsigned)(high >> 32),
        (unsigned)((high >> 16) & 0xFFFF),
        (unsigned)(high & 0xFFFF),
        (unsigned)(low >> 48),
        (unsigned long long)(low & 0x0000FFFFFFFFFFFFULL));
    return String(buf);
}

EID::EID(const String &str) {
    if (str.size() == 0) {
        created_at = 0;
        id = 0;
        return;
    }
    if (str.size() == 16) {
        created_at = 0;
        id = 0;
        const u8 *bytes = (const u8*)str.data();
        for (int i = 0; i < 8; i++) {
            created_at = (created_at << 8) | bytes[i];
            id = (id << 8) | bytes[8 + i];
        }
        return;
    }
    u8 bytes[16] = {0};
    usz n = decodeCB32BigEndian(str, bytes, 16);
    if (n == 16) {
        created_at = 0;
        id = 0;
        for (int i = 0; i < 8; i++) {
            created_at = (created_at << 8) | bytes[i];
            id = (id << 8) | bytes[8 + i];
        }
    } else {
        created_at = 0;
        id = 0;
    }
}

EID::EID(const char *string) : EID(string ? String(string) : String()) {}

EID EID::random() {
    u64 r = 0;
    randomFill((u8*)&r, sizeof(r));
    return EID(epochMillis(), r);
}

String EID::toString() const {
    u8 bytes[16];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (u8)((created_at >> ((7 - i) * 8)) & 0xFF);
        bytes[8 + i] = (u8)((id >> ((7 - i) * 8)) & 0xFF);
    }
    return encodeCB32BigEndian(bytes, 16);
}

String EID::toBinary() const {
    u8 bytes[16];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (u8)((created_at >> ((7 - i) * 8)) & 0xFF);
        bytes[8 + i] = (u8)((id >> ((7 - i) * 8)) & 0xFF);
    }
    return String(bytes, 16);
}

} // namespace Ksee

