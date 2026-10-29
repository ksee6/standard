/**
 * @file FS.cpp
 * @brief Implementation of Unified Virtual Filesystem (FS) and Linux OSFilesystem.
 */

#include <Ksee/Resource/FS.hpp>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

namespace Ksee {
namespace Resource {

// Global singleton definition
OSFilesystem fs;

String FS::normalize(const String& path) {
    if (path.isEmpty()) return "/";

    Array<String> parts;
    String cur;

    for (usz i = 0; i < path.size(); ++i) {
        char c = (char)path[i];
        if (c == '/') {
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

String FS::resolve(const Path& target) const {
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
        while (cleanRoot.size() > 1 && cleanRoot.endsWith("/")) {
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

usz FS::write(const Path& target, const String& content, u64 offset) {
    Handle h = open(target);
    if (h.isClosed()) return 0;
    h.seek((usz)offset);
    h.write(content);
    h.close();
    return content.size();
}

usz FS::append(const Path& target, const String& content) {
    Handle h = open(target);
    if (h.isClosed()) return 0;
    h.seek(h.size);
    h.write(content);
    h.close();
    return content.size();
}

String FS::read(const Path& target, u64 offset, u64 length) {
    Handle h = open(target);
    if (h.isClosed()) return "";
    h.seek((usz)offset);
    String res = h.read((usz)length);
    h.close();
    return res;
}

static bool makeParentDirectories(const String& path) {
    long long lastSlash = -1;
    for (long long i = (long long)path.size() - 1; i >= 0; --i) {
        if (path[(usz)i] == '/') {
            lastSlash = i;
            break;
        }
    }
    if (lastSlash <= 0) return true;

    String parent = path.substring(0, (usz)lastSlash);
    struct stat st;
    if (stat(parent.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }

    makeParentDirectories(parent);
    ::mkdir(parent.c_str(), 0755);
    return true;
}

Handle OSFilesystem::open(const Path& target) {
    String osPath = resolve(target);

    // Ensure parent directory exists for creation
    makeParentDirectories(osPath);

    int fd = ::open(osPath.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        fd = ::open(osPath.c_str(), O_RDONLY);
    }

    if (fd >= 0) {
        Handle h(fd, target.toString());
        h.filePath = osPath;
        return h;
    }

    Handle closedH;
    closedH.destroy();
    return closedH;
}

bool OSFilesystem::signal(const Path& target, const Path& source, u64 sig) {
    String osTarget = resolve(target);
    String osSource = source.isEmpty() ? "" : resolve(source);

    switch (sig) {
        case FS::Unlink: {
            return ::unlink(osTarget.c_str()) == 0;
        }
        case FS::Move: {
            if (osSource.isEmpty()) return false;
            makeParentDirectories(osTarget);
            return ::rename(osSource.c_str(), osTarget.c_str()) == 0;
        }
        case FS::Copy: {
            if (osSource.isEmpty()) return false;
            int srcFd = ::open(osSource.c_str(), O_RDONLY);
            if (srcFd < 0) return false;

            makeParentDirectories(osTarget);
            int dstFd = ::open(osTarget.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (dstFd < 0) {
                ::close(srcFd);
                return false;
            }

            char buf[4096];
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
        case FS::Symlink: {
            if (osSource.isEmpty()) return false;
            makeParentDirectories(osTarget);
            return ::symlink(osSource.c_str(), osTarget.c_str()) == 0;
        }
        case FS::Mkdir: {
            makeParentDirectories(osTarget);
            return ::mkdir(osTarget.c_str(), 0755) == 0;
        }
        case FS::Rmdir: {
            return ::rmdir(osTarget.c_str()) == 0;
        }
        default:
            return false;
    }
}

} // namespace Resource
} // namespace Ksee
