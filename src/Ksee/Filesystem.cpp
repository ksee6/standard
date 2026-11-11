/**
 * @file Filesystem.cpp
 * @brief Implementation of Unified Virtual Filesystem (Filesystem) and cross-platform OSFilesystem.
 */

#include <Ksee/Filesystem.hpp>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#endif

namespace Ksee {

// Global singleton definition
OSFilesystem fs;

String Filesystem::normalize(const String& path) {
    if (path.isEmpty()) return "/";

    Array<String> parts;
    String cur;

    for (usz i = 0; i < path.size(); ++i) {
        char c = (char)path[i];
        if (c == '/' || c == '\\') {
            if (cur.size() > 0) {
                if (cur == ".") {
                    // Current directory: ignore
                } else if (cur == "..") {
                    if (parts.size() > 0) parts.pop();
                } else {
                    parts.push(cur);
                }
                cur = "";
            }
        } else {
            cur.push((u8)c);
        }
    }

    if (cur.size() > 0) {
        if (cur == ".") {
            // ignore
        } else if (cur == "..") {
            if (parts.size() > 0) parts.pop();
        } else {
            parts.push(cur);
        }
    }

    if (parts.size() == 0) return "/";

    String result;
    for (usz i = 0; i < parts.size(); ++i) {
        result.push('/');
        result.append(parts[i].data(), parts[i].size());
    }
    return result;
}

String Filesystem::resolve(const Path& target) const {
    String raw = target.toString();
    String normTarget = normalize(raw);

    // Apply preamble inside the sandbox
    if (!preamble.isEmpty()) {
        String normPre = normalize(preamble);
        if (normPre != "/") {
            if (normTarget == "/") {
                normTarget = normPre;
            } else {
                normTarget = normPre + normTarget;
            }
        }
    }

    // Apply machine root prefix (jail/containment)
    if (!root.isEmpty()) {
        String cleanRoot = root;
        while (cleanRoot.size() > 1 && (cleanRoot.endsWith("/") || cleanRoot.endsWith("\\"))) {
            cleanRoot = cleanRoot.substring(0, cleanRoot.size() - 1);
        }

        String full = cleanRoot + normTarget;
        // Verify containment: must start with root
        if (!full.startsWith(cleanRoot)) {
            full = cleanRoot + "/";
        }
        return full;
    }

    return normTarget;
}

usz Filesystem::write(const Path& target, const String& content, u64 offset) {
    u64 h = open(target, true);
    if (h == 0) return 0;
    if (offset > 0) {
        seek(h, offset);
    }
    usz written = write(h, content.data(), content.size());
    close(h);
    return written;
}

usz Filesystem::append(const Path& target, const String& content) {
    u64 h = open(target, true);
    if (h == 0) return 0;
    u64 curSize = size(h);
    seek(h, curSize);
    usz written = write(h, content.data(), content.size());
    close(h);
    return written;
}

String Filesystem::read(const Path& target, u64 offset, u64 length) {
    u64 h = open(target, false);
    if (h == 0) return "";
    u64 fileSize = size(h);
    if (offset >= fileSize) {
        close(h);
        return "";
    }
    u64 toRead = (length == 0 || offset + length > fileSize) ? (fileSize - offset) : length;
    seek(h, offset);
    String result;
    char buf[4096];
    u64 remaining = toRead;
    while (remaining > 0) {
        usz chunk = remaining > sizeof(buf) ? sizeof(buf) : (usz)remaining;
        usz n = read(h, buf, chunk);
        if (n == 0) break;
        result.append(buf, n);
        remaining -= n;
    }
    close(h);
    return result;
}

static bool makeParentDirectories(const String& path) {
    long long lastSlash = -1;
    for (long long i = (long long)path.size() - 1; i >= 0; --i) {
        char c = (char)path[(usz)i];
        if (c == '/' || c == '\\') {
            lastSlash = i;
            break;
        }
    }
    if (lastSlash <= 0) return true;

    String parent = path.substring(0, (usz)lastSlash);
#if defined(_WIN32)
    DWORD attrs = GetFileAttributesA(parent.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    makeParentDirectories(parent);
    CreateDirectoryA(parent.c_str(), NULL);
    return true;
#else
    struct stat st;
    if (stat(parent.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    makeParentDirectories(parent);
    ::mkdir(parent.c_str(), 0755);
    return true;
#endif
}

// -----------------------------------------------------------------------------
// OSFilesystem Implementation
// -----------------------------------------------------------------------------

#if defined(_WIN32)

u64 OSFilesystem::open(const Path& target, bool writeMode) {
    String osPath = resolve(target);
    if (writeMode) {
        makeParentDirectories(osPath);
    }
    DWORD access = writeMode ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
    DWORD disposition = writeMode ? OPEN_ALWAYS : OPEN_EXISTING;
    HANDLE h = CreateFileA(osPath.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        if (writeMode) {
            h = CreateFileA(osPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        if (h == INVALID_HANDLE_VALUE) return 0;
    }
    return (u64)(uintptr_t)h;
}

void OSFilesystem::close(u64 handle) {
    if (handle == 0) return;
    CloseHandle((HANDLE)(uintptr_t)handle);
}

usz OSFilesystem::read(u64 handle, void* dest, usz bytes) {
    if (handle == 0 || !dest || bytes == 0) return 0;
    DWORD readBytes = 0;
    if (ReadFile((HANDLE)(uintptr_t)handle, dest, (DWORD)bytes, &readBytes, NULL)) {
        return (usz)readBytes;
    }
    return 0;
}

usz OSFilesystem::write(u64 handle, const void* src, usz bytes) {
    if (handle == 0 || !src || bytes == 0) return 0;
    DWORD written = 0;
    if (WriteFile((HANDLE)(uintptr_t)handle, src, (DWORD)bytes, &written, NULL)) {
        return (usz)written;
    }
    return 0;
}

u64 OSFilesystem::seek(u64 handle, u64 offset) {
    if (handle == 0) return 0;
    LARGE_INTEGER li, res;
    li.QuadPart = (LONGLONG)offset;
    if (SetFilePointerEx((HANDLE)(uintptr_t)handle, li, &res, FILE_BEGIN)) {
        return (u64)res.QuadPart;
    }
    return 0;
}

u64 OSFilesystem::size(u64 handle) {
    if (handle == 0) return 0;
    LARGE_INTEGER res;
    if (GetFileSizeEx((HANDLE)(uintptr_t)handle, &res)) {
        return (u64)res.QuadPart;
    }
    return 0;
}

bool OSFilesystem::exists(const Path& target) {
    String osTarget = resolve(target);
    return GetFileAttributesA(osTarget.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool OSFilesystem::isDir(const Path& target) {
    String osTarget = resolve(target);
    DWORD attrs = GetFileAttributesA(osTarget.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool OSFilesystem::isFile(const Path& target) {
    String osTarget = resolve(target);
    DWORD attrs = GetFileAttributesA(osTarget.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

Array<Path> OSFilesystem::list(const Path& target) {
    Array<Path> results;
    String osTarget = resolve(target);
    String query = osTarget + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(query.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return results;
    do {
        if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
            Path p = target;
            p.push(String(fd.cFileName));
            results.push(p);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return results;
}

bool OSFilesystem::unlink(const Path& target) {
    String osTarget = resolve(target);
    return DeleteFileA(osTarget.c_str()) != 0;
}

bool OSFilesystem::move(const Path& target, const Path& source) {
    if (source.isEmpty()) return false;
    String osTarget = resolve(target);
    String osSource = resolve(source);
    makeParentDirectories(osTarget);
    return MoveFileExA(osSource.c_str(), osTarget.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

bool OSFilesystem::copy(const Path& target, const Path& source) {
    if (source.isEmpty()) return false;
    String osTarget = resolve(target);
    String osSource = resolve(source);
    makeParentDirectories(osTarget);
    return CopyFileA(osSource.c_str(), osTarget.c_str(), FALSE) != 0;
}

bool OSFilesystem::symlink(const Path& target, const Path& source) {
    if (source.isEmpty()) return false;
    String osTarget = resolve(target);
    String osSource = resolve(source);
    makeParentDirectories(osTarget);
    return CreateSymbolicLinkA(osTarget.c_str(), osSource.c_str(), 0) != 0;
}

bool OSFilesystem::mkdir(const Path& target) {
    String osTarget = resolve(target);
    makeParentDirectories(osTarget);
    return CreateDirectoryA(osTarget.c_str(), NULL) != 0;
}

bool OSFilesystem::rmdir(const Path& target) {
    String osTarget = resolve(target);
    return RemoveDirectoryA(osTarget.c_str()) != 0;
}

#else // Linux / POSIX

u64 OSFilesystem::open(const Path& target, bool writeMode) {
    String osPath = resolve(target);
    if (writeMode) {
        makeParentDirectories(osPath);
    }
    int flags = writeMode ? (O_RDWR | O_CREAT) : O_RDONLY;
    int fd = ::open(osPath.c_str(), flags, 0666);
    if (fd < 0 && writeMode) {
        fd = ::open(osPath.c_str(), O_WRONLY | O_CREAT, 0666);
    }
    if (fd < 0) return 0;
    return (u64)(fd + 1); // 0 indicates invalid, 1..N maps to fd
}

void OSFilesystem::close(u64 handle) {
    if (handle == 0) return;
    ::close((int)(handle - 1));
}

usz OSFilesystem::read(u64 handle, void* dest, usz bytes) {
    if (handle == 0 || !dest || bytes == 0) return 0;
    ssize_t n = ::read((int)(handle - 1), dest, (size_t)bytes);
    return n > 0 ? (usz)n : 0;
}

usz OSFilesystem::write(u64 handle, const void* src, usz bytes) {
    if (handle == 0 || !src || bytes == 0) return 0;
    ssize_t n = ::write((int)(handle - 1), src, (size_t)bytes);
    return n > 0 ? (usz)n : 0;
}

u64 OSFilesystem::seek(u64 handle, u64 offset) {
    if (handle == 0) return 0;
    off_t res = ::lseek((int)(handle - 1), (off_t)offset, SEEK_SET);
    return res >= 0 ? (u64)res : 0;
}

u64 OSFilesystem::size(u64 handle) {
    if (handle == 0) return 0;
    struct stat st;
    if (::fstat((int)(handle - 1), &st) == 0) {
        return (u64)st.st_size;
    }
    return 0;
}

bool OSFilesystem::exists(const Path& target) {
    String osTarget = resolve(target);
    struct stat st;
    return ::stat(osTarget.c_str(), &st) == 0;
}

bool OSFilesystem::isDir(const Path& target) {
    String osTarget = resolve(target);
    struct stat st;
    return ::stat(osTarget.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool OSFilesystem::isFile(const Path& target) {
    String osTarget = resolve(target);
    struct stat st;
    return ::stat(osTarget.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

Array<Path> OSFilesystem::list(const Path& target) {
    Array<Path> results;
    String osTarget = resolve(target);
    DIR *d = ::opendir(osTarget.c_str());
    if (!d) return results;
    struct dirent *ent;
    while ((ent = ::readdir(d)) != nullptr) {
        if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
            Path p = target;
            p.push(String(ent->d_name));
            results.push(p);
        }
    }
    ::closedir(d);
    return results;
}

bool OSFilesystem::unlink(const Path& target) {
    String osTarget = resolve(target);
    return ::unlink(osTarget.c_str()) == 0;
}

bool OSFilesystem::move(const Path& target, const Path& source) {
    if (source.isEmpty()) return false;
    String osTarget = resolve(target);
    String osSource = resolve(source);
    makeParentDirectories(osTarget);
    return ::rename(osSource.c_str(), osTarget.c_str()) == 0;
}

bool OSFilesystem::copy(const Path& target, const Path& source) {
    if (source.isEmpty()) return false;
    String osTarget = resolve(target);
    String osSource = resolve(source);

    int srcFd = ::open(osSource.c_str(), O_RDONLY);
    if (srcFd < 0) return false;

    makeParentDirectories(osTarget);
    int dstFd = ::open(osTarget.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (dstFd < 0) {
        ::close(srcFd);
        return false;
    }

    char buf[8192];
    ssize_t bytes;
    bool success = true;
    while ((bytes = ::read(srcFd, buf, sizeof(buf))) > 0) {
        ssize_t written = ::write(dstFd, buf, (size_t)bytes);
        if (written != bytes) {
            success = false;
            break;
        }
    }
    if (bytes < 0) success = false;

    ::close(srcFd);
    ::close(dstFd);
    return success;
}

bool OSFilesystem::symlink(const Path& target, const Path& source) {
    if (source.isEmpty()) return false;
    String osTarget = resolve(target);
    String osSource = resolve(source);
    makeParentDirectories(osTarget);
    return ::symlink(osSource.c_str(), osTarget.c_str()) == 0;
}

bool OSFilesystem::mkdir(const Path& target) {
    String osTarget = resolve(target);
    makeParentDirectories(osTarget);
    return ::mkdir(osTarget.c_str(), 0755) == 0;
}

bool OSFilesystem::rmdir(const Path& target) {
    String osTarget = resolve(target);
    return ::rmdir(osTarget.c_str()) == 0;
}

#endif

} // namespace Ksee
