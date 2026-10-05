/**
 * @file Tar.cpp
 * @brief POSIX ustar TAR archive packer and unpacker implementation.
 */

#include "../../../include/Ksee/Compression/Tar.hpp"
#include <stdio.h>
#include <string.h>

namespace Ksee {

static void formatOctal(char *dst, usz len, u64 value) {
    snprintf(dst, len, "%0*llo", (int)(len - 1), (unsigned long long)value);
}

static u64 parseOctal(const char *src, usz len) {
    u64 res = 0;
    usz i = 0;
    while (i < len && (src[i] == ' ' || src[i] == '\0')) i++;
    while (i < len && src[i] >= '0' && src[i] <= '7') {
        res = (res << 3) + (src[i] - '0');
        i++;
    }
    return res;
}

String Tar::pack(const Array<TarEntry> &entries) {
    String out;
    for (usz e = 0; e < entries.size(); ++e) {
        const TarEntry &entry = entries[e];
        char header[512];
        memset(header, 0, sizeof(header));

        // Name (100)
        strncpy(header, entry.name.c_str(), 100);

        // Mode (8)
        formatOctal(header + 100, 8, entry.mode);

        // UID (8), GID (8)
        formatOctal(header + 108, 8, 1000);
        formatOctal(header + 116, 8, 1000);

        // Size (12)
        u64 dataSize = entry.isDir ? 0 : entry.data.size();
        formatOctal(header + 124, 12, dataSize);

        // Mtime (12)
        formatOctal(header + 136, 12, 0);

        // Chksum placeholder (8 spaces)
        memset(header + 148, ' ', 8);

        // Typeflag (1)
        header[156] = entry.isDir ? '5' : '0';

        // Magic (6) + Version (2)
        memcpy(header + 257, "ustar\0", 6);
        memcpy(header + 263, "00", 2);

        // Calculate checksum
        u32 chksum = 0;
        for (int i = 0; i < 512; ++i) {
            chksum += (u8)header[i];
        }
        formatOctal(header + 148, 7, chksum);
        header[155] = ' ';

        out.pushEach((const u8*)header, 512);

        if (!entry.isDir && dataSize > 0) {
            out.pushEach((const u8*)entry.data.data(), dataSize);
            usz pad = (512 - (dataSize % 512)) % 512;
            if (pad > 0) {
                u8 zeros[512] = {0};
                out.pushEach(zeros, pad);
            }
        }
    }

    // End-of-archive marker (two 512-byte zero blocks)
    u8 endZeros[1024] = {0};
    out.pushEach(endZeros, 1024);
    return out;
}

Array<TarEntry> Tar::unpack(const String &tarBlob) {
    Array<TarEntry> entries;
    const u8 *ptr = (const u8*)tarBlob.data();
    usz remaining = tarBlob.size();

    while (remaining >= 512) {
        // Check for two consecutive empty blocks
        bool allZero = true;
        for (int i = 0; i < 512; ++i) {
            if (ptr[i] != 0) {
                allZero = false;
                break;
            }
        }
        if (allZero) break;

        const char *hdr = (const char*)ptr;
        TarEntry entry;

        char nameBuf[101] = {0};
        memcpy(nameBuf, hdr, 100);
        entry.name = String(nameBuf);

        entry.mode = (u32)parseOctal(hdr + 100, 8);
        u64 size = parseOctal(hdr + 124, 12);
        char typeflag = hdr[156];
        entry.isDir = (typeflag == '5') || entry.name.endsWith("/");

        ptr += 512;
        remaining -= 512;

        if (!entry.isDir && size > 0) {
            if (size > remaining) break;
            entry.data.pushEach(ptr, size);
            usz pad = (512 - (size % 512)) % 512;
            ptr += size + pad;
            remaining -= (size + pad);
        }

        entries.push(entry);
    }

    return entries;
}

} // namespace Ksee

