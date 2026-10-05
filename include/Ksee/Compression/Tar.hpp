/**
 * @file Tar.hpp
 * @brief POSIX ustar TAR archive packer and unpacker for Ksee.
 */

#ifndef KSEE_COMPRESSION_TAR_HPP
#define KSEE_COMPRESSION_TAR_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "../Array.hpp"

namespace Ksee {

struct TarEntry {
    String name;
    String data;
    u32 mode = 0644;
    bool isDir = false;
};

class Tar {
public:
    static String pack(const Array<TarEntry> &entries);
    static Array<TarEntry> unpack(const String &tarBlob);
};

} // namespace Ksee

#endif // KSEE_COMPRESSION_TAR_HPP

