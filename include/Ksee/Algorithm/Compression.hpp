/**
 * @file Compression.hpp
 * @brief High-performance standalone DEFLATE / INFLATE compression engine for Ksee.
 *        Fully compliant with RFC 1951, RFC 1950 (zlib), and RFC 1952 (gzip).
 *        Features Dynamic Huffman coding, LZ77 sliding window, and universal container detection.
 */

#ifndef KSEE_ALGORITHM_COMPRESSION_HPP
#define KSEE_ALGORITHM_COMPRESSION_HPP

#include "../Ksee.hpp"
#include "../String.hpp"

namespace Ksee {
namespace Algorithm {

/**
 * @brief Compresses input string using DEFLATE with Dynamic Huffman and LZ77.
 * @param input Raw uncompressed bytes.
 * @param level Compression level (0 = uncompressed, 1 = fastest, 6 = default, 9 = maximum).
 * @return Compressed byte string.
 */
String deflate(const String &input, i32 level = 6);

/**
 * @brief Decompresses input string using INFLATE.
 *        Automatically detects and inflates raw DEFLATE (RFC 1951), zlib (RFC 1950),
 *        and gzip (RFC 1952) streams.
 * @param input Compressed byte string.
 * @return Decompressed raw byte string, or empty string on corrupt input.
 */
String inflate(const String &input);

} // namespace Algorithm
} // namespace Ksee

#endif // KSEE_ALGORITHM_COMPRESSION_HPP
