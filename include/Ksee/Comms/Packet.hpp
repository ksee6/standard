/**
 * @file Packet.hpp
 * @brief Standard universal Packet primitive for Ksee (IPC, Filesystems, Actors, and Network Transport).
 */

#ifndef KSEE_COMMS_PACKET_HPP
#define KSEE_COMMS_PACKET_HPP

#include "../Ksee.hpp"
#include "../String.hpp"

namespace Ksee {
namespace Comms {

/**
 * @struct Packet
 * @brief Standard universal Packet primitive for Ksee (IPC, Filesystems, Actors, and Network Transport).
 */
struct Packet {
    u64 handle = 1;            ///< Handle
    u64 intent = 1;            ///< Semantic intent
    u64 offset = 0;            ///< Byte offset for I/O and sliced operations
    u64 intendedLength = 0;
    bool reliable = true;      ///< Retransmit on loss until acknowledged?
    bool ordered = true;       ///< Deliver in strict sequence per-handle?
    bool sacrificial = false;  ///< Safe to gamble on unverified exploratory bands?
    String payload;            ///< Packet payload bytes

    Packet() = default;

    // Full constructor: payload, handle, intent, offset, length, delivery flags
    Packet(const String &p, u64 h, u64 in, u64 off, u64 len, bool rel = true, bool ord = true, bool sac = false)
        : handle(h), intent(in), offset(off), intendedLength(len), reliable(rel), ordered(ord), sacrificial(sac), payload(p) {}

    Packet(const char *p, u64 h, u64 in, u64 off, u64 len, bool rel = true, bool ord = true, bool sac = false)
        : handle(h), intent(in), offset(off), intendedLength(len), reliable(rel), ordered(ord), sacrificial(sac), payload(p) {}

    // Convenience: payload, handle, intent, delivery flags
    Packet(const String &p, u64 h, u64 in, bool rel, bool ord = true, bool sac = false)
        : handle(h), intent(in), offset(0), intendedLength(0), reliable(rel), ordered(ord), sacrificial(sac), payload(p) {}

    Packet(const char *p, u64 h, u64 in, bool rel, bool ord = true, bool sac = false)
        : handle(h), intent(in), offset(0), intendedLength(0), reliable(rel), ordered(ord), sacrificial(sac), payload(p) {}

    // Convenience: payload, handle
    Packet(const String &p, u64 h = 1)
        : handle(h), intent(1), offset(0), intendedLength(0), reliable(true), ordered(true), sacrificial(false), payload(p) {}

    Packet(const char *p, u64 h = 1)
        : handle(h), intent(1), offset(0), intendedLength(0), reliable(true), ordered(true), sacrificial(false), payload(p) {}
};

} // namespace Comms
} // namespace Ksee

#endif // KSEE_COMMS_PACKET_HPP
