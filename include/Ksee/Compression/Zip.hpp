/**
 * @file Zip.hpp
 * @brief ZIP archive packing and unpacking for Ksee.
 */

#ifndef KSEE_COMPRESSION_ZIP_HPP
#define KSEE_COMPRESSION_ZIP_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "../Array.hpp"

namespace Ksee {

struct ZipEntry {
    String name;
    String data;
    bool isDir = false;
};

class Zip {
public:
    static String pack(const Array<ZipEntry> &entries, int level = 6);
    static Array<ZipEntry> unpack(const String &zipBlob);
};

} // namespace Ksee

#endif // KSEE_COMPRESSION_ZIP_HPP

