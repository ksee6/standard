/**
 * @file ID.hpp
 * @brief Identifiers module for Ksee, containing UUID (v7 and v4).
 */

#ifndef KSEE_ID_HPP
#define KSEE_ID_HPP

#include "Types.hpp"
#include "String.hpp"

namespace Ksee {

struct UUID {
    union {
        struct { u64 high; u64 low; };
        struct { u64 hi; u64 lo; };
    };

    UUID();
    UUID(const String &string);
    UUID(const char *string);
    UUID(u64 high, u64 low);

    static UUID v7();
    static UUID v4();
    static UUID random() { return v4(); }
    static UUID nil() { return UUID(0, 0); }
    bool isNil() const { return high == 0 && low == 0; }

    String toString() const;

    bool operator==(const UUID &o) const { return high == o.high && low == o.low; }
    bool operator!=(const UUID &o) const { return !(*this == o); }
    bool operator<(const UUID &o) const {
        if (high != o.high) return high < o.high;
        return low < o.low;
    }
};

struct EID {
    union {
        struct { u64 created_at; u64 id; };
        struct { u64 high; u64 low; };
        struct { u64 hi; u64 lo; };
    };

    constexpr EID() : created_at(0), id(0) {}
    EID(u64 id) : created_at(epochMillis()), id(id) {}
    EID(u64 created_at, u64 id) : created_at(created_at), id(id) {}
    EID(const String &string);
    EID(const char *string);

    static EID fromId(u64 id) { return EID(epochMillis(), id); }
    static EID random();
    static EID nil() { return EID(0, 0); }

    bool isNil() const { return created_at == 0 && id == 0; }

    String toString() const;

    explicit operator bool() const { return !isNil(); }

    bool operator==(const EID &o) const { return created_at == o.created_at && id == o.id; }
    bool operator!=(const EID &o) const { return !(*this == o); }
    bool operator<(const EID &o) const {
        if (created_at != o.created_at) return created_at < o.created_at;
        return id < o.id;
    }
    bool operator>(const EID &o) const { return o < *this; }
    bool operator<=(const EID &o) const { return !(o < *this); }
    bool operator>=(const EID &o) const { return !(*this < o); }
};

inline EID id(u64 idVal) { return EID(idVal); }
inline EID eid(u64 idVal) { return EID(idVal); }

} // namespace Ksee

#endif // KSEE_ID_HPP

