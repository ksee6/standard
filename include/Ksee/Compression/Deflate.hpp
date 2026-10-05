/**
 * @file Deflate.hpp
 * @brief DEFLATE / zlib compression for Ksee.
 */

#ifndef KSEE_COMPRESSION_DEFLATE_HPP
#define KSEE_COMPRESSION_DEFLATE_HPP

#include "../Types.hpp"
#include "../String.hpp"

namespace Ksee {

class Deflate {
public:
    int level = 6;
    usz maxScratch = 1024 * 1024 * 128;

    Deflate() = default;

    static String compress(const String &input, int level = 6);
    static String decompress(const String &input, usz maxScratch = 1024 * 1024 * 128);
};

} // namespace Ksee

#endif // KSEE_COMPRESSION_DEFLATE_HPP

