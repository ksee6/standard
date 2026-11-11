/**
 * @file Filesystem.hpp
 * @brief Unified Virtual Filesystem (Filesystem) and OSFilesystem interface for Ksee.
 */

#ifndef KSEE_FILESYSTEM_HPP
#define KSEE_FILESYSTEM_HPP

#include "Ksee.hpp"
#include "String.hpp"
#include "Path.hpp"

namespace Ksee {

/**
 * @class Filesystem
 * @brief Abstract Virtual Filesystem with sandboxed containment, root, and preamble prefixes.
 */
class Filesystem {
public:
    String root;     ///< Sandbox base directory on host (never escapable).
    String preamble; ///< Virtual prefix visible inside the sandbox.

    virtual ~Filesystem() = default;

    // Raw descriptor / stream I/O (0 indicates failure / invalid handle)
    virtual u64 open(const Path& target, bool writeMode = false) = 0;
    virtual void close(u64 handle) = 0;
    virtual usz read(u64 handle, void* dest, usz bytes) = 0;
    virtual usz write(u64 handle, const void* src, usz bytes) = 0;
    virtual u64 seek(u64 handle, u64 offset) = 0;
    virtual u64 size(u64 handle) = 0;

    // Default file streaming implementations
    virtual usz write(const Path& target, const String& content, u64 offset = 0);
    virtual usz append(const Path& target, const String& content);
    virtual String read(const Path& target, u64 offset = 0, u64 length = 0);

    // Direct filesystem virtual operations
    virtual bool exists(const Path& target) = 0;
    virtual bool isDir(const Path& target) = 0;
    virtual bool isFile(const Path& target) = 0;
    virtual Array<Path> list(const Path& target) = 0;

    virtual bool unlink(const Path& target) = 0;
    virtual bool move(const Path& target, const Path& source) = 0;
    virtual bool copy(const Path& target, const Path& source) = 0;
    virtual bool symlink(const Path& target, const Path& source) = 0;
    virtual bool mkdir(const Path& target) = 0;
    virtual bool rmdir(const Path& target) = 0;

    // Path resolution & sandbox confinement
    String resolve(const Path& target) const;
    static String normalize(const String& path);
    static String normalise(const String& path) { return normalize(path); }
};

/**
 * @class OSFilesystem
 * @brief Concrete host filesystem implementation adhering to the Filesystem interface.
 */
class OSFilesystem : public Filesystem {
public:
    OSFilesystem() = default;
    virtual ~OSFilesystem() = default;

    using Filesystem::read;
    using Filesystem::write;

    u64 open(const Path& target, bool writeMode = false) override;
    void close(u64 handle) override;
    usz read(u64 handle, void* dest, usz bytes) override;
    usz write(u64 handle, const void* src, usz bytes) override;
    u64 seek(u64 handle, u64 offset) override;
    u64 size(u64 handle) override;

    bool exists(const Path& target) override;
    bool isDir(const Path& target) override;
    bool isFile(const Path& target) override;
    Array<Path> list(const Path& target) override;

    bool unlink(const Path& target) override;
    bool move(const Path& target, const Path& source) override;
    bool copy(const Path& target, const Path& source) override;
    bool symlink(const Path& target, const Path& source) override;
    bool mkdir(const Path& target) override;
    bool rmdir(const Path& target) override;
};

// Global default filesystem singleton
extern OSFilesystem fs;

} // namespace Ksee

#endif // KSEE_FILESYSTEM_HPP
