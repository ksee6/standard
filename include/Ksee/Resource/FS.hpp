/**
 * @file FS.hpp
 * @brief Unified Virtual Filesystem (FS) and OSFilesystem interface for Ksee.
 */

#ifndef KSEE_RESOURCE_FS_HPP
#define KSEE_RESOURCE_FS_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "../Path.hpp"
#include "Handle.hpp"

#ifndef KSEE_EXPORT
#define KSEE_EXPORT
#endif

namespace Ksee {
namespace Resource {

/**
 * @class FS
 * @brief Abstract Virtual Filesystem with sandboxed containment, root, and preamble prefixes.
 */
class KSEE_EXPORT FS {
public:
    String root;     ///< Full path on machine, subtracted and never escapable.
    String preamble; ///< Path appended after root, visible inside, never escapable.

    virtual ~FS() = default;

    // Signal action codes
    static constexpr u64 Unlink  = 0;
    static constexpr u64 Move    = 1;
    static constexpr u64 Copy    = 2;
    static constexpr u64 Symlink = 3;
    static constexpr u64 Mkdir   = 4;
    static constexpr u64 Rmdir   = 5;

    // Core abstract I/O methods
    virtual Handle open(const Path& target) = 0;
    virtual bool signal(const Path& target, const Path& source = Path(), u64 sig = 0) = 0;

    // Default file streaming implementations
    virtual usz write(const Path& target, const String& content, u64 offset = 0);
    virtual usz append(const Path& target, const String& content);
    virtual String read(const Path& target, u64 offset = 0, u64 length = 0);

    // Convenience signal wrappers
    bool unlink(const Path& target) { return signal(target, Path(), Unlink); }
    bool mkdir(const Path& target) { return signal(target, Path(), Mkdir); }
    bool rmdir(const Path& target) { return signal(target, Path(), Rmdir); }
    bool move(const Path& target, const Path& source) { return signal(target, source, Move); }
    bool symlink(const Path& target, const Path& source) { return signal(target, source, Symlink); }
    bool copy(const Path& target, const Path& source) { return signal(target, source, Copy); }

    // Path resolution & sandbox confinement
    String resolve(const Path& target) const;
    static String normalize(const String& path);
};

/**
 * @class OSFilesystem
 * @brief Concrete Linux host filesystem implementation adhering to the FS interface.
 */
class KSEE_EXPORT OSFilesystem : public FS {
public:
    OSFilesystem() = default;
    virtual ~OSFilesystem() = default;

    Handle open(const Path& target) override;
    bool signal(const Path& target, const Path& source = Path(), u64 sig = 0) override;
};

// Global default filesystem singleton
extern KSEE_EXPORT OSFilesystem fs;

} // namespace Resource

using Resource::FS;
using Resource::OSFilesystem;
using Resource::fs;

} // namespace Ksee

#endif // KSEE_RESOURCE_FS_HPP
