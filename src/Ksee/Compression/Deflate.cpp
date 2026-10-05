/**
 * @file Deflate.cpp
 * @brief DEFLATE / zlib compression implementation.
 */

#include "../../../include/Ksee/Compression/Deflate.hpp"
#include <string.h>

#if __has_include(<zlib.h>)
#include <zlib.h>
#define KSEE_ZLIB_AVAILABLE 1
#elif __has_include(<miniz.h>)
#include <miniz.h>
#define KSEE_MINIZ_AVAILABLE 1
#elif __has_include("miniz.h")
#include "miniz.h"
#define KSEE_MINIZ_AVAILABLE 1
#endif

namespace Ksee {

String Deflate::compress(const String &input, int level) {
    if (input.isEmpty()) return String();
#if defined(KSEE_ZLIB_AVAILABLE)
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    if (deflateInit(&strm, level) != Z_OK) return String();

    uLong bound = deflateBound(&strm, (uLong)input.size());
    String output;
    u8 *buf = new u8[bound];
    strm.next_in = (Bytef*)input.data();
    strm.avail_in = (uInt)input.size();
    strm.next_out = buf;
    strm.avail_out = (uInt)bound;

    int ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END && ret != Z_OK) {
        deflateEnd(&strm);
        delete[] buf;
        return String();
    }
    uLong destLen = bound - strm.avail_out;
    deflateEnd(&strm);
    output.pushEach(buf, (usz)destLen);
    delete[] buf;
    return output;
#elif defined(KSEE_MINIZ_AVAILABLE)
    mz_ulong bound = mz_compressBound((mz_ulong)input.size());
    String output;
    output.allocate((usz)bound);

    mz_ulong destLen = bound;
    int ret = mz_compress2(output.data(), &destLen, input.data(),
                           (mz_ulong)input.size(), level);
    if (ret != MZ_OK) return String();

    output.allocate((usz)destLen);
    return output;
#else
    (void)level;
    return input;
#endif
}

String Deflate::decompress(const String &input, usz maxScratch) {
    if (input.isEmpty()) return String();
#if defined(KSEE_ZLIB_AVAILABLE)
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    if (inflateInit(&strm) != Z_OK) return String();

    strm.next_in = (Bytef*)input.data();
    strm.avail_in = (uInt)input.size();

    usz cap = input.size() * 4;
    if (cap < 64) cap = 64;
    if (cap > maxScratch) cap = maxScratch;

    String output;
    u8 *buf = new u8[cap];
    strm.next_out = buf;
    strm.avail_out = (uInt)cap;

    int ret = inflate(&strm, Z_NO_FLUSH);
    while (ret == Z_OK || ret == Z_BUF_ERROR) {
        if (strm.avail_out == 0) {
            usz oldCap = cap;
            cap *= 2;
            if (cap > maxScratch) {
                inflateEnd(&strm);
                delete[] buf;
                return String();
            }
            u8 *newBuf = new u8[cap];
            memcpy(newBuf, buf, oldCap);
            delete[] buf;
            buf = newBuf;
            strm.next_out = buf + oldCap;
            strm.avail_out = (uInt)(cap - oldCap);
        }
        ret = inflate(&strm, Z_FINISH);
    }
    usz totalOut = (usz)strm.total_out;
    inflateEnd(&strm);
    output.pushEach(buf, totalOut);
    delete[] buf;
    return output;
#elif defined(KSEE_MINIZ_AVAILABLE)
    mz_stream stream;
    memset(&stream, 0, sizeof(stream));

    int ret = mz_inflateInit(&stream);
    if (ret != MZ_OK) return String();

    stream.next_in = input.data();
    stream.avail_in = (unsigned int)input.size();

    usz outCap = input.size() * 4;
    if (outCap < 64) outCap = 64;
    if (outCap > maxScratch) outCap = maxScratch;

    String output;
    output.allocate(outCap);
    stream.next_out = output.data();
    stream.avail_out = (unsigned int)outCap;

    while (true) {
        ret = mz_inflate(&stream, MZ_FINISH);
        if (ret == MZ_STREAM_END) break;
        if (ret == MZ_BUF_ERROR || ret == MZ_OK) {
            usz oldCap = outCap;
            outCap *= 2;
            if (outCap > maxScratch) {
                mz_inflateEnd(&stream);
                return String();
            }
            output.allocate(outCap);
            stream.next_out = output.data() + oldCap;
            stream.avail_out = (unsigned int)(outCap - oldCap);
        } else {
            mz_inflateEnd(&stream);
            return String();
        }
    }

    output.allocate((usz)stream.total_out);
    mz_inflateEnd(&stream);
    return output;
#else
    (void)maxScratch;
    return input;
#endif
}

} // namespace Ksee
