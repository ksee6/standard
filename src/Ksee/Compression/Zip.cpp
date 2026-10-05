/**
 * @file Zip.cpp
 * @brief Standalone ZIP archive packing and unpacking implementation.
 */

#include "../../../include/Ksee/Compression/Zip.hpp"
#include "../../../include/Ksee/Compression/Deflate.hpp"
#include <string.h>

namespace Ksee {

static inline void write16_le(u8 *p, u16 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
}

static inline void write32_le(u8 *p, u32 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

static inline u16 read16_le(const u8 *p) {
    return (u16)p[0] | ((u16)p[1] << 8);
}

static inline u32 read32_le(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u32 crc32_zip(const u8 *data, usz length) {
    u32 crc = 0xFFFFFFFF;
    for (usz i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

String Zip::pack(const Array<ZipEntry> &entries, int level) {
    String out;
    Array<u32> localOffsets;
    Array<u32> crcs;
    Array<String> compressedDatas;
    Array<u16> methods;

    // Write Local File Headers + Data
    for (usz i = 0; i < entries.size(); ++i) {
        const ZipEntry &e = entries[i];
        localOffsets.push((u32)out.size());

        u32 crc = e.data.isEmpty() ? 0 : crc32_zip((const u8*)e.data.data(), e.data.size());
        crcs.push(crc);

        String compData;
        u16 method = 0; // store
        if (level > 0 && !e.data.isEmpty()) {
            compData = Deflate::compress(e.data, level);
            if (!compData.isEmpty() && compData.size() < e.data.size()) {
                method = 8; // deflate
            } else {
                compData = e.data;
                method = 0;
            }
        } else {
            compData = e.data;
            method = 0;
        }
        methods.push(method);
        compressedDatas.push(compData);

        u8 lfh[30];
        lfh[0] = 'P'; lfh[1] = 'K'; lfh[2] = 3; lfh[3] = 4;
        write16_le(lfh + 4, 20);            // version needed
        write16_le(lfh + 6, 0);             // flags
        write16_le(lfh + 8, method);        // compression method
        write16_le(lfh + 10, 0);            // mod time
        write16_le(lfh + 12, 0);            // mod date
        write32_le(lfh + 14, crc);          // crc32
        write32_le(lfh + 18, (u32)compData.size()); // compressed size
        write32_le(lfh + 22, (u32)e.data.size());   // uncompressed size
        write16_le(lfh + 26, (u16)e.name.size());   // filename length
        write16_le(lfh + 28, 0);            // extra field length

        out.pushEach(lfh, 30);
        out.pushEach((const u8*)e.name.data(), e.name.size());
        if (!compData.isEmpty()) {
            out.pushEach((const u8*)compData.data(), compData.size());
        }
    }

    u32 cdOffset = (u32)out.size();

    // Write Central Directory Headers
    for (usz i = 0; i < entries.size(); ++i) {
        const ZipEntry &e = entries[i];
        u8 cdh[46];
        cdh[0] = 'P'; cdh[1] = 'K'; cdh[2] = 1; cdh[3] = 2;
        write16_le(cdh + 4, 20);                    // version made by
        write16_le(cdh + 6, 20);                    // version needed
        write16_le(cdh + 8, 0);                     // flags
        write16_le(cdh + 10, methods[i]);           // method
        write16_le(cdh + 12, 0);                    // mod time
        write16_le(cdh + 14, 0);                    // mod date
        write32_le(cdh + 16, crcs[i]);              // crc32
        write32_le(cdh + 20, (u32)compressedDatas[i].size()); // comp size
        write32_le(cdh + 24, (u32)e.data.size());   // uncomp size
        write16_le(cdh + 28, (u16)e.name.size());   // filename len
        write16_le(cdh + 30, 0);                    // extra len
        write16_le(cdh + 32, 0);                    // comment len
        write16_le(cdh + 34, 0);                    // disk start
        write16_le(cdh + 36, 0);                    // internal attr
        write32_le(cdh + 38, e.isDir ? 0x10 : 0);   // external attr
        write32_le(cdh + 42, localOffsets[i]);       // local header offset

        out.pushEach(cdh, 46);
        out.pushEach((const u8*)e.name.data(), e.name.size());
    }

    u32 cdSize = (u32)(out.size() - cdOffset);

    // End of Central Directory Record (EOCD)
    u8 eocd[22];
    eocd[0] = 'P'; eocd[1] = 'K'; eocd[2] = 5; eocd[3] = 6;
    write16_le(eocd + 4, 0);                        // disk number
    write16_le(eocd + 6, 0);                        // disk with start of CD
    write16_le(eocd + 8, (u16)entries.size());       // entries on this disk
    write16_le(eocd + 10, (u16)entries.size());      // total entries in CD
    write32_le(eocd + 12, cdSize);                  // size of CD
    write32_le(eocd + 16, cdOffset);                // offset of CD
    write16_le(eocd + 20, 0);                       // comment len

    out.pushEach(eocd, 22);
    return out;
}

Array<ZipEntry> Zip::unpack(const String &zipBlob) {
    Array<ZipEntry> res;
    if (zipBlob.size() < 22) return res;

    const u8 *data = (const u8*)zipBlob.data();
    usz n = zipBlob.size();

    // Find EOCD by scanning backwards
    long long eocdPos = -1;
    for (long long i = (long long)n - 22; i >= 0 && i >= (long long)n - 65557; --i) {
        if (data[i] == 'P' && data[i + 1] == 'K' && data[i + 2] == 5 && data[i + 3] == 6) {
            eocdPos = i;
            break;
        }
    }
    if (eocdPos < 0) return res;

    u16 numEntries = read16_le(data + eocdPos + 10);
    u32 cdOffset = read32_le(data + eocdPos + 16);

    usz ptr = cdOffset;
    for (u16 e = 0; e < numEntries && ptr + 46 <= n; ++e) {
        if (data[ptr] != 'P' || data[ptr + 1] != 'K' || data[ptr + 2] != 1 || data[ptr + 3] != 2) {
            break;
        }
        u16 method = read16_le(data + ptr + 10);
        u32 compSize = read32_le(data + ptr + 20);
        u32 uncompSize = read32_le(data + ptr + 24);
        u16 nameLen = read16_le(data + ptr + 28);
        u16 extraLen = read16_le(data + ptr + 30);
        u16 commentLen = read16_le(data + ptr + 32);
        u32 extAttr = read32_le(data + ptr + 38);
        u32 localOffset = read32_le(data + ptr + 42);

        String name;
        if (ptr + 46 + nameLen <= n) {
            name.pushEach(data + ptr + 46, nameLen);
        }

        bool isDir = (extAttr & 0x10) != 0 || name.endsWith("/");

        // Locate file data from local header
        String fileData;
        if (localOffset + 30 <= n && !isDir) {
            u16 locNameLen = read16_le(data + localOffset + 26);
            u16 locExtraLen = read16_le(data + localOffset + 28);
            usz dataStart = localOffset + 30 + locNameLen + locExtraLen;
            if (dataStart + compSize <= n) {
                String rawComp;
                rawComp.pushEach(data + dataStart, compSize);
                if (method == 0) {
                    fileData = rawComp;
                } else if (method == 8) {
                    fileData = Deflate::decompress(rawComp);
                } else {
                    fileData = rawComp;
                }
            }
        }

        res.push(ZipEntry{name, fileData, isDir});
        ptr += 46 + nameLen + extraLen + commentLen;
    }

    return res;
}

} // namespace Ksee

