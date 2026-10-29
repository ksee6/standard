/**
 * @file Handle.cpp
 * @brief Implementation of high-performance virtual stream, OS descriptor, and IPC handle.
 */

#include <Ksee/Resource/Handle.hpp>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace Ksee {
namespace Resource {

HandleCore::HandleCore()
    : id(0), ownerId(0), refCount(0), blocked(false), allowMoreThanTwo(false),
      isReady(false), size(0), priority(0), options(0), fd(-1), autoCloseFd(true) {}

HandleCore::~HandleCore() {
    for (usz i = 0; i < packets.size(); ++i) {
        if (packets[i]) delete packets[i];
    }
    packets.clear();
    if (fd >= 0 && autoCloseFd) {
        ::close(fd);
        fd = -1;
    }
}

Handle::Handle() {
    path = "";
    id = 0;
    ownerId = 0;
    offset = 0;
    position = 0;
    writePosition = 0;
    closed = false;
    blocking = true;
    flags = 0;
    sharable = false;
    logicalSize = 0;
    maxCachedBytes = 0;
    filePath = "";
    size = 0;
    priority = 0;
    options = 0;
    source = "";
    core = new HandleCore();
    core->refCount = 1;
}

Handle::Handle(int osFd, const String& boundPath) : Handle() {
    if (core) {
        core->fd = osFd;
        core->path = boundPath;
    }
    this->path = boundPath;
    this->filePath = boundPath;
    if (osFd >= 0) {
        struct stat st;
        if (fstat(osFd, &st) == 0) {
            this->size = (usz)st.st_size;
            this->writePosition = this->size;
            if (core) core->size = this->size;
        }
    } else {
        closed = true;
    }
}

void Handle::copyFrom(const Handle& o) {
    size = o.size;
    priority = o.priority;
    options = o.options;
    source = o.source;
    id = o.id;
    ownerId = o.ownerId;
    sharable = o.sharable;
    path = o.path;
    filePath = o.filePath;
    offset = o.offset;
    pointer = o.pointer;
    position = o.position;
    writePosition = o.writePosition;
    closed = o.closed;
    blocking = o.blocking;
    flags = o.flags;
    logicalSize = o.logicalSize;
    maxCachedBytes = o.maxCachedBytes;
    currentAuthor = o.currentAuthor;
    onReadyCb = o.onReadyCb;
    onLeaveCb = o.onLeaveCb;
    onSignalCb = o.onSignalCb;
    onWriteCb = o.onWriteCb;
    onReadCb = o.onReadCb;

    core = o.core;
    if (core) {
        core->refCount++;
    }
}

void Handle::moveFrom(Handle&& o) noexcept {
    size = o.size;
    priority = o.priority;
    options = o.options;
    source = static_cast<String&&>(o.source);
    id = o.id;
    ownerId = o.ownerId;
    sharable = o.sharable;
    path = static_cast<String&&>(o.path);
    filePath = static_cast<String&&>(o.filePath);
    offset = o.offset;
    pointer = o.pointer;
    position = o.position;
    writePosition = o.writePosition;
    closed = o.closed;
    blocking = o.blocking;
    flags = o.flags;
    logicalSize = o.logicalSize;
    maxCachedBytes = o.maxCachedBytes;
    currentAuthor = o.currentAuthor;
    onReadyCb = static_cast<Func<void(void*)>&&>(o.onReadyCb);
    onLeaveCb = static_cast<Func<void(void*)>&&>(o.onLeaveCb);
    onSignalCb = static_cast<Func<void(void*, u32, String)>&&>(o.onSignalCb);
    onWriteCb = static_cast<Func<void(Packet*)>&&>(o.onWriteCb);
    onReadCb = static_cast<Func<void(usz, usz)>&&>(o.onReadCb);

    core = o.core;
    o.core = nullptr;
    o.closed = true;
}

Handle::Handle(const Handle& o) {
    copyFrom(o);
}

Handle::Handle(Handle&& o) noexcept {
    moveFrom(static_cast<Handle&&>(o));
}

Handle& Handle::operator=(const Handle& o) {
    if (this != &o) {
        destroy();
        copyFrom(o);
    }
    return *this;
}

Handle& Handle::operator=(Handle&& o) noexcept {
    if (this != &o) {
        destroy();
        moveFrom(static_cast<Handle&&>(o));
    }
    return *this;
}

Handle::~Handle() {
    destroy();
}

usz Handle::read(void* dest, usz reqSize) {
    String s = read(reqSize);
    if (s.size() > 0 && dest) {
        __builtin_memcpy(dest, s.data(), s.size());
        return s.size();
    }
    return 0;
}

usz Handle::write(const void* src, usz reqSize) {
    if (!src || reqSize == 0) return 0;
    String s((const u8*)src, reqSize);
    write(s);
    return reqSize;
}

void Handle::write(const char* s) {
    if (s) write(String(s));
}

void Handle::dummyWrite(usz pos, usz len) {
    if (len == 0) return;
    String zeroes;
    for (usz i = 0; i < len; ++i) zeroes.push('\0');
    write(zeroes, pos);
}

void Handle::setCaching(usz bytes) {
    maxCachedBytes = bytes;
}

void Handle::checkEviction() {}

void Handle::flush() {
    if (core) {
        for (usz i = 0; i < core->packets.size(); ++i) {
            delete core->packets[i];
        }
        core->packets.clear();
    }
    offset = 0;
    position = 0;
    writePosition = 0;
}

void Handle::flush(usz pos, usz length) {
    if (!core || length == 0) return;
    usz flush_start = pos;
    usz flush_end = pos + length;

    for (long long i = (long long)core->packets.size() - 1; i >= 0; --i) {
        Packet* pp = core->packets[(usz)i];
        if (!pp) continue;
        usz pStart = pp->position;
        usz pEnd = pp->position + pp->size();

        if (pEnd <= flush_start || pStart >= flush_end) continue;
        if (pStart >= flush_start && pEnd <= flush_end) {
            remove(pp);
            continue;
        }
        if (pStart < flush_start && pEnd <= flush_end) {
            usz keepLen = flush_start - pStart;
            if (pp->size() > keepLen) *pp = pp->substring(0, keepLen);
            continue;
        }
        if (pStart >= flush_start && pStart < flush_end && pEnd > flush_end) {
            usz diff = flush_end - pStart;
            pp->position = flush_end;
            if (pp->size() >= diff) *pp = pp->substring(diff);
            continue;
        }
    }
}

usz Handle::available() {
    if (!core) return 0;
    if (core->fd >= 0) {
        struct stat st;
        if (fstat(core->fd, &st) == 0) {
            if ((usz)st.st_size > offset) return (usz)st.st_size - offset;
        }
        return 0;
    }
    usz maxByte = 0;
    for (usz i = 0; i < core->packets.size(); ++i) {
        Packet* pp = core->packets[i];
        if (pp) {
            usz end = pp->position + pp->size();
            if (end > maxByte) maxByte = end;
        }
    }
    if (maxByte > offset) return maxByte - offset;
    return 0;
}

Array<Handle::Packet*> Handle::select(usz length, usz off) {
    if (!core) return {};
    if (onReadCb) onReadCb(length, off);
    for (usz i = 0; i < core->onReadCbs.size(); ++i) {
        if (core->onReadCbs[i]) core->onReadCbs[i](length, off);
    }

    Array<Packet*> result;
    for (usz i = 0; i < core->packets.size(); ++i) {
        Packet* pp = core->packets[i];
        if (!pp) continue;
        bool match = false;
        if (length == 0) {
            if (pp->position + pp->size() > off) match = true;
        } else {
            if (pp->position + pp->size() > off && pp->position < off + length) match = true;
        }
        if (match) {
            result.push(pp);
        }
    }
    return result;
}

void Handle::remove(Packet* pp) {
    if (!core || !pp) return;
    for (long long i = (long long)core->packets.size() - 1; i >= 0; --i) {
        if (core->packets[(usz)i] == pp) {
            usz last = core->packets.size() - 1;
            if ((usz)i != last) core->packets[(usz)i] = core->packets[last];
            core->packets.pop();
            delete pp;
            break;
        }
    }
}

String Handle::read(usz length, usz off) {
    if (!core) return "";

    // OS-backed descriptor branch
    if (core->fd >= 0) {
        usz readLen = length;
        if (readLen == 0) {
            struct stat st;
            if (fstat(core->fd, &st) == 0 && (usz)st.st_size > off) {
                readLen = (usz)st.st_size - off;
            }
        }
        if (readLen == 0) return "";

        u8* buf = new u8[readLen];
        ssize_t n = ::pread(core->fd, buf, readLen, (off_t)off);
        if (n <= 0) {
            delete[] buf;
            return "";
        }
        this->offset = off + (usz)n;
        this->position = this->offset;
        String res(buf, (usz)n);
        delete[] buf;
        return res;
    }

    // In-memory / IPC packets branch
    Array<Packet*> pkts = select(length, off);
    if (pkts.size() == 0) return "";

    usz startOff = off;
    usz endOff = (length == 0) ? 0 : (off + length);
    if (length == 0) {
        for (usz i = 0; i < pkts.size(); ++i) {
            usz pe = pkts[i]->position + pkts[i]->size();
            if (pe > endOff) endOff = pe;
        }
    }

    if (endOff <= startOff) return "";
    usz totalLen = endOff - startOff;

    Array<u8> buf;
    Array<bool> filled;
    for (usz i = 0; i < totalLen; ++i) {
        buf.push(0);
        filled.push(false);
    }

    for (long long idx = (long long)pkts.size() - 1; idx >= 0; --idx) {
        Packet* pp = pkts[(usz)idx];
        if (!pp) continue;
        usz pStart = pp->position;
        usz pEnd = pp->position + pp->size();
        const u8* src = (const u8*)pp->c_str();
        if (!src) continue;

        usz rStart = (pStart > startOff) ? pStart : startOff;
        usz rEnd = (pEnd < endOff) ? pEnd : endOff;
        if (rStart < rEnd) {
            for (usz b = rStart; b < rEnd; ++b) {
                usz outIdx = b - startOff;
                if (!filled[outIdx]) {
                    usz srcIdx = b - pStart;
                    buf[outIdx] = src[srcIdx];
                    filled[outIdx] = true;
                }
            }
        }
    }

    for (usz i = 0; i < pkts.size(); ++i) {
        Packet* pp = pkts[i];
        if (!pp) continue;
        usz pStart = pp->position;
        usz pEnd = pp->position + pp->size();
        if (pStart >= startOff && pEnd <= endOff) {
            remove(pp);
        } else if (pStart < startOff && pEnd <= endOff) {
            usz newLen = startOff - pStart;
            if (pp->size() > newLen) {
                *pp = pp->substring(0, newLen);
            }
        } else if (pStart >= startOff && pStart < endOff && pEnd > endOff) {
            usz consumed = endOff - pStart;
            pp->position = endOff;
            pp->consumed += consumed;
            if (pp->size() >= consumed) {
                *pp = pp->substring(consumed);
            }
            if (pp->size() == 0) {
                remove(pp);
            }
        }
    }

    this->offset = startOff + totalLen;
    this->position = this->offset;
    return String(reinterpret_cast<const u8*>(buf.data()), totalLen);
}

String Handle::read(usz length) {
    return read(length, this->offset);
}

String Handle::peek(usz length, usz off) {
    if (!core) return "";

    if (core->fd >= 0) {
        usz readLen = length;
        if (readLen == 0) {
            struct stat st;
            if (fstat(core->fd, &st) == 0 && (usz)st.st_size > off) {
                readLen = (usz)st.st_size - off;
            }
        }
        if (readLen == 0) return "";
        u8* buf = new u8[readLen];
        ssize_t n = ::pread(core->fd, buf, readLen, (off_t)off);
        if (n <= 0) {
            delete[] buf;
            return "";
        }
        String res(buf, (usz)n);
        delete[] buf;
        return res;
    }

    Array<Packet*> pkts = select(length, off);
    if (pkts.size() == 0) return "";

    usz startOff = off;
    usz endOff = (length == 0) ? 0 : (off + length);
    if (length == 0) {
        for (usz i = 0; i < pkts.size(); ++i) {
            usz pe = pkts[i]->position + pkts[i]->size();
            if (pe > endOff) endOff = pe;
        }
    }

    if (endOff <= startOff) return "";
    usz totalLen = endOff - startOff;

    Array<u8> buf;
    Array<bool> filled;
    for (usz i = 0; i < totalLen; ++i) {
        buf.push(0);
        filled.push(false);
    }

    for (long long idx = (long long)pkts.size() - 1; idx >= 0; --idx) {
        Packet* pp = pkts[(usz)idx];
        if (!pp) continue;
        usz pStart = pp->position;
        usz pEnd = pp->position + pp->size();
        const u8* src = (const u8*)pp->c_str();
        if (!src) continue;

        usz rStart = (pStart > startOff) ? pStart : startOff;
        usz rEnd = (pEnd < endOff) ? pEnd : endOff;
        if (rStart < rEnd) {
            for (usz b = rStart; b < rEnd; ++b) {
                usz outIdx = b - startOff;
                if (!filled[outIdx]) {
                    usz srcIdx = b - pStart;
                    buf[outIdx] = src[srcIdx];
                    filled[outIdx] = true;
                }
            }
        }
    }

    return String(reinterpret_cast<const u8*>(buf.data()), totalLen);
}

String Handle::peek(usz length) {
    return peek(length, this->offset);
}

void Handle::onWrite(Func<void(Packet*)> fn) {
    onWriteCb = fn;
    if (core) core->onWriteCbs.push(fn);
}

void Handle::onRead(Func<void(usz, usz)> fn) {
    onReadCb = fn;
    if (core) core->onReadCbs.push(fn);
}

void Handle::onReady(Func<void(void*)> fn) {
    onReadyCb = fn;
    if (core) core->onReadyCbs.push(fn);
}

void Handle::onLeave(Func<void(void*)> fn) {
    onLeaveCb = fn;
    if (core) core->onLeaveCbs.push(fn);
}

void Handle::onSignal(Func<void(void*, u32, String)> fn) {
    onSignalCb = fn;
    if (core) core->onSignalCbs.push(fn);
}

void Handle::signal(u32 sig, const String& meta) {
    if (core && sig == 1) { // Symlink
        core->symlinkTarget = meta;
    }
    if (onSignalCb) onSignalCb(nullptr, sig, meta);
    if (core) {
        for (usz i = 0; i < core->onSignalCbs.size(); ++i) {
            if (core->onSignalCbs[i]) core->onSignalCbs[i](nullptr, sig, meta);
        }
    }
}

void Handle::write(const String& data, usz off, ActorId author) {
    if (!core) return;

    if (core->fd >= 0) {
        ssize_t n = ::pwrite(core->fd, data.data(), data.size(), (off_t)off);
        if (n > 0) {
            this->offset = off + (usz)n;
            this->position = this->offset;
            this->writePosition = this->offset;
            if (this->offset > this->size) {
                this->size = this->offset;
                core->size = this->size;
            }
        }
        return;
    }

    Packet* pp = new Packet();
    *pp = data;
    pp->position = off;
    pp->author = author;
    pp->path = this->path;
    pp->consumed = 0;

    if (off + data.size() > this->writePosition) {
        this->writePosition = off + data.size();
    }
    core->packets.push(pp);

    if (onWriteCb) onWriteCb(pp);
    for (usz i = 0; i < core->onWriteCbs.size(); ++i) {
        if (core->onWriteCbs[i]) core->onWriteCbs[i](pp);
    }
}

void Handle::write(const String& data, usz off) {
    ActorId a = (this->currentAuthor != 0) ? this->currentAuthor : this->ownerId;
    write(data, off, a);
}

void Handle::writeAuthor(const String& data, ActorId author) {
    write(data, this->writePosition, author);
}

void Handle::write(const String& data) {
    ActorId a = (this->currentAuthor != 0) ? this->currentAuthor : this->ownerId;
    write(data, this->writePosition, a);
}

void Handle::write(const Packet& packet) {
    write(static_cast<const String&>(packet), packet.position, packet.author);
}

void Handle::write(usz pointerVal, usz len, usz off, ActorId author) {
    if (!core) return;
    String data;
    if (pointerVal != 0 && len > 0) {
        data = String(reinterpret_cast<const u8*>(pointerVal), len);
    }
    write(data, off, author);
}

void Handle::write(usz pointerVal, usz len, usz off) {
    ActorId a = (this->currentAuthor != 0) ? this->currentAuthor : this->ownerId;
    write(pointerVal, len, off, a);
}

void Handle::writeAuthor(usz pointerVal, usz len, ActorId author) {
    write(pointerVal, len, this->writePosition, author);
}

void Handle::write(usz pointerVal, usz len) {
    ActorId a = (this->currentAuthor != 0) ? this->currentAuthor : this->ownerId;
    write(pointerVal, len, this->writePosition, a);
}

void Handle::ephemeralWrite(const String& data, usz off) {
    write(data, off);
}

void Handle::ephemeralWrite(const String& data) {
    ephemeralWrite(data, this->writePosition);
}

void Handle::ephemeralWrite(usz pointerVal, usz len, usz off) {
    write(pointerVal, len, off);
}

void Handle::ephemeralWrite(usz pointerVal, usz len) {
    ephemeralWrite(pointerVal, len, this->writePosition);
}

void Handle::close() {
    destroy();
}

bool Handle::isClosed() const {
    return closed;
}

void Handle::seek(usz off) {
    this->offset = off;
    this->position = off;
    this->writePosition = off;
    if (core && core->fd >= 0) {
        ::lseek(core->fd, (off_t)off, SEEK_SET);
    }
}

void Handle::setBlocking(bool b) {
    blocking = b;
    if (!b) flags |= 0x800; // O_NONBLOCK
    else flags &= ~0x800;
}

usz Handle::tell() const {
    return this->offset;
}

usz Handle::getOffset() const {
    return this->offset;
}

void Handle::bind(const String& newPath) {
    this->path = newPath;
    this->filePath = newPath;
    if (core) core->path = newPath;
}

void Handle::unbind() {
    this->path = "";
    this->filePath = "";
    if (core) core->path = "";
}

void Handle::wait() {}

Handle::Packet Handle::readp() {
    if (!core) return Packet();
    while (core->packets.size() == 0 && !closed) {
        wait();
    }
    if (core->packets.size() == 0) return Packet();
    Packet* p = core->packets[0];
    core->packets.remove(0);
    Packet ret = *p;
    if (this->offset < p->position + p->size()) {
        this->offset = p->position + p->size();
        this->position = this->offset;
    }
    delete p;
    return ret;
}

void Handle::ready() {
    if (!core) return;
    if (size > 0) core->size = size;
    else if (core->size > 0) size = core->size;
    if (priority > 0) core->priority = priority;
    else if (core->priority > 0) priority = core->priority;
    if (options > 0) core->options = options;
    else if (core->options > 0) options = core->options;
    if (source.size() > 0) core->source = source;
    else if (core->source.size() > 0) source = core->source;

    if (core->isReady) return;
    core->isReady = true;

    if (onReadyCb) onReadyCb(nullptr);
    for (usz i = 0; i < core->onReadyCbs.size(); ++i) {
        if (core->onReadyCbs[i]) core->onReadyCbs[i](nullptr);
    }
}

void Handle::destroy() {
    if (closed) return;
    closed = true;
    if (onLeaveCb) onLeaveCb(nullptr);
    if (core) {
        for (usz i = 0; i < core->onLeaveCbs.size(); ++i) {
            if (core->onLeaveCbs[i]) core->onLeaveCbs[i](nullptr);
        }
        if (core->refCount > 0) core->refCount--;
        if (core->refCount == 0) {
            delete core;
        }
        core = nullptr;
    }
}

String Handle::next() {
    if (available() > 0) {
        return readp();
    }
    return "";
}

String Handle::operator*() {
    if (!core || core->packets.size() == 0) return "";
    return *core->packets[0];
}

} // namespace Resource
} // namespace Ksee
