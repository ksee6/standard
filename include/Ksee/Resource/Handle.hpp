/**
 * @file Handle.hpp
 * @brief High-performance virtual stream, OS descriptor, IPC handle and packet abstraction for Ksee.
 */

#ifndef KSEE_RESOURCE_HANDLE_HPP
#define KSEE_RESOURCE_HANDLE_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "../Array.hpp"
#include "../Func.hpp"
#include "../Iterator.hpp"
#include "../Path.hpp"
#include <type_traits>
#include <tuple>

#ifndef KSEE_EXPORT
#define KSEE_EXPORT
#endif

namespace Ksee {
namespace Resource {

namespace Detail {
    template <typename F>
    struct CallbackTraits : CallbackTraits<decltype(&F::operator())> {};

    template <typename R, typename C, typename Arg1, typename... Args>
    struct CallbackTraits<R(C::*)(Arg1, Args...) const> {
        using FirstArg = Arg1;
    };

    template <typename R, typename C, typename Arg1, typename... Args>
    struct CallbackTraits<R(C::*)(Arg1, Args...)> {
        using FirstArg = Arg1;
    };

    template <typename R, typename Arg1, typename... Args>
    struct CallbackTraits<R(*)(Arg1, Args...)> {
        using FirstArg = Arg1;
    };

    template <typename R, typename C>
    struct CallbackTraits<R(C::*)() const> {
        using FirstArg = void*;
    };

    template <typename R, typename C>
    struct CallbackTraits<R(C::*)()> {
        using FirstArg = void*;
    };
}

using ActorId = u64;

namespace HandleTypes {
    /**
     * @struct Packet
     * @brief A Handle packet extending String with provenance and stream positioning.
     */
    struct Packet : public String {
        ActorId author = 0;   ///< Unforgeable provenance / Actor ID (0 = invisible / system / external)
        usz position = 0;     ///< Offset in stream
        String path;          ///< Bound path (empty if unbound)
        usz consumed = 0;     ///< Number of bytes already consumed from this packet

        Packet() = default;
        Packet(const String& s) : String(s) {}
        explicit Packet(const char* s) : String(s) {}

        Packet(const Packet& o)
            : String(o), author(o.author), position(o.position), path(o.path),
              consumed(o.consumed) {}

        Packet(Packet&& o) noexcept
            : String(static_cast<String&&>(o)), author(o.author), position(o.position),
              path(static_cast<String&&>(o.path)), consumed(o.consumed) {}

        Packet& operator=(const Packet& o) {
            if (this != &o) {
                static_cast<String&>(*this) = o;
                author = o.author;
                position = o.position;
                path = o.path;
                consumed = o.consumed;
            }
            return *this;
        }

        Packet& operator=(Packet&& o) noexcept {
            if (this != &o) {
                static_cast<String&>(*this) = static_cast<String&&>(o);
                author = o.author;
                position = o.position;
                path = static_cast<String&&>(o.path);
                consumed = o.consumed;
            }
            return *this;
        }

        Packet& operator=(const String& s) {
            static_cast<String&>(*this) = s;
            return *this;
        }
        Packet& operator=(const char* s) {
            return (*this = String(s));
        }

        usz offset() const { return position; }
        usz length() const { return size(); }
        const u8* ptr() const { return (const u8*)c_str(); }

        bool operator==(const char* s) const { return static_cast<const String&>(*this) == s; }
        bool operator==(const String& s) const { return static_cast<const String&>(*this) == s; }
        bool operator!=(const char* s) const { return !(*this == s); }
        bool operator!=(const String& s) const { return !(*this == s); }
    };
}

using Packet = HandleTypes::Packet;

struct HandleCore {
    String path;
    usz id = 0;
    usz ownerId = 0;
    usz refCount = 0;
    bool blocked = false;
    bool allowMoreThanTwo = false;
    bool isReady = false;
    String symlinkTarget;
    Array<void*> openers;
    Array<HandleTypes::Packet*> packets;

    usz size = 0;
    u32 priority = 0;
    u32 options = 0;
    String source;

    int fd = -1;           ///< OS file descriptor (-1 for virtual/IPC handle)
    bool autoCloseFd = true;

    Array<Func<void(HandleTypes::Packet*)>> onWriteCbs;
    Array<Func<void(usz, usz)>> onReadCbs;
    Array<Func<void(void*)>> onReadyCbs;
    Array<Func<void(void*)>> onLeaveCbs;
    Array<Func<void(void*, u32, String)>> onSignalCbs;

    HandleCore();
    ~HandleCore();
};

/**
 * @class Handle
 * @brief Zero-copy, capability-aware virtual stream, OS descriptor & IPC primitive, extending Iterator<String, String>.
 */
class KSEE_EXPORT Handle : public Iterator<String, String> {
public:
    using Packet = HandleTypes::Packet;

    // Handle metadata fields
    usz size = 0;
    u32 priority = 0;
    u32 options = 0;
    String source;
    usz id = 0;
    usz ownerId = 0;
    bool sharable = false;
    String path;
    String filePath;
    usz offset = 0;
    usz pointer = 0;
    usz position = 0;
    usz writePosition = 0;
    bool closed = false;
    bool blocking = true;
    int flags = 0;
    usz logicalSize = 0;
    usz maxCachedBytes = 0;
    HandleCore* core = nullptr;

    Func<void(void*)> onReadyCb;
    Func<void(void*)> onLeaveCb;
    Func<void(void*, u32, String)> onSignalCb;
    Func<void(Packet*)> onWriteCb;
    Func<void(usz, usz)> onReadCb;

    ActorId currentAuthor = 0;

    Handle();
    Handle(int osFd, const String& boundPath = "");

    // Rule of Five: Copy and Move Semantics
    Handle(const Handle& o);
    Handle(Handle&& o) noexcept;
    Handle& operator=(const Handle& o);
    Handle& operator=(Handle&& o) noexcept;

    virtual ~Handle();

    virtual usz read(void* dest, usz size);
    virtual usz write(const void* src, usz size);
    virtual void write(const char* s);
    virtual void dummyWrite(usz pos, usz len);
    virtual void setCaching(usz bytes);
    virtual void checkEviction();
    virtual void flush();
    virtual void flush(usz pos, usz length);
    virtual usz available() override;
    virtual Array<Packet*> select(usz length = 0, usz off = 0);
    virtual void remove(Packet* pp);
    virtual String read(usz length, usz off);
    virtual String read(usz length = 0);
    virtual String peek(usz length, usz off);
    virtual String peek(usz length = 0);

    virtual void onWrite(Func<void(Packet*)> fn);
    virtual void onRead(Func<void(usz, usz)> fn);
    virtual void onReady(Func<void(void*)> fn);

    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Func<void(void*)>>>>
    void onReady(F&& fn) {
        using Arg0 = typename Detail::CallbackTraits<std::decay_t<F>>::FirstArg;
        onReady(Func<void(void*)>([f = std::forward<F>(fn)](void* t) {
            f(reinterpret_cast<Arg0>(t));
        }));
    }

    virtual void onLeave(Func<void(void*)> fn);

    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Func<void(void*)>>>>
    void onLeave(F&& fn) {
        using Arg0 = typename Detail::CallbackTraits<std::decay_t<F>>::FirstArg;
        onLeave(Func<void(void*)>([f = std::forward<F>(fn)](void* t) {
            f(reinterpret_cast<Arg0>(t));
        }));
    }

    virtual void onSignal(Func<void(void*, u32, String)> fn);

    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Func<void(void*, u32, String)>>>>
    void onSignal(F&& fn) {
        using Arg0 = typename Detail::CallbackTraits<std::decay_t<F>>::FirstArg;
        onSignal(Func<void(void*, u32, String)>([f = std::forward<F>(fn)](void* t, u32 sig, String meta) {
            f(reinterpret_cast<Arg0>(t), sig, meta);
        }));
    }

    virtual void signal(u32 sig = 0, const String& meta = "");

    virtual void write(const String& data, usz off, ActorId author);
    virtual void write(const String& data, usz off);
    virtual void writeAuthor(const String& data, ActorId author);
    virtual void write(const String& data);
    virtual void write(const Packet& packet);
    virtual void write(usz pointer, usz len, usz off, ActorId author);
    virtual void write(usz pointer, usz len, usz off);
    virtual void writeAuthor(usz pointer, usz len, ActorId author);
    virtual void write(usz pointer, usz len);
    virtual void ephemeralWrite(const String& data, usz off);
    virtual void ephemeralWrite(const String& data);
    virtual void ephemeralWrite(usz pointer, usz len, usz off);
    virtual void ephemeralWrite(usz pointer, usz len);

    virtual void close();
    virtual bool isClosed() const;
    virtual void seek(usz off);
    virtual void setBlocking(bool b);
    virtual usz tell() const;
    virtual usz getOffset() const;
    virtual void bind(const String& newPath);
    virtual void unbind();
    virtual void wait();
    virtual Packet readp();
    virtual void ready();
    virtual void destroy();

    // Iterator interface
    String next() override;
    String operator*() override;

private:
    void copyFrom(const Handle& o);
    void moveFrom(Handle&& o) noexcept;
};

} // namespace Resource
using Resource::Handle;
using Resource::HandleCore;
using Resource::Packet;
} // namespace Ksee

#endif // KSEE_RESOURCE_HANDLE_HPP
