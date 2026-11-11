/**
 * @file Comms.hpp
 * @brief Unified Communications primitives for Ksee: Standard Packet and Physical Band.
 */

#ifndef KSEE_RESOURCE_COMMS_HPP
#define KSEE_RESOURCE_COMMS_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "../Array.hpp"
#include "../Func.hpp"
#include "../Math/Random.hpp"

namespace Ksee {
namespace Resource {
namespace Comms {

/**
 * @struct Packet
 * @brief Standard universal Packet primitive for Ksee (IPC, Filesystems, Actors, and Network Transport).
 */
struct Packet {
    u64 handle = 1;            ///< Stream / virtual channel handle (defaults to 1)
    u64 intent = 1;            ///< Action / semantic verb (1 = Standard Data)
    u64 offset = 0;            ///< Byte offset for I/O and sliced operations
    u64 intendedLength = 0;    ///< Total intended length of the operation (0 = unlimited / full payload)
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

/**
 * @class Band
 * @brief Represents a physical transmission medium/channel with duty cycle limits, debt regeneration, and shared slot dispatch.
 */
class Band {
public:
    u64 lastDebtRegenTime = 0;     ///< Deducts maxDuty from duty every second
    u64 duty = 0;                  ///< Current debt in ms; may exceed maxDuty while transmitting a full packet
    u64 maxDuty = 0;               ///< Duty cycle limit in ms per second (0 = infinite)
    u64 stunnedUntil = 0;          ///< Backoff timestamp in millis
    u64 minStunTime = 1;           ///< Minimum randomized stun recovery in ms (default 1)
    u64 maxStunTime = 10;          ///< Maximum randomized stun recovery in ms (default 10)
    u64 idealCapacity = 0;         ///< Defaults to 0; favors curiosity packets stochastically
    u64 maxBundleSize = 1400;      ///< Maximum bundle size allowed (0 = infinite)

    Func<String()> recvCallback;
    Func<void(String)> sendCallback;
    Func<void(String)> onRecv;

    // Slot hooks for protocol attachments (e.g. LinesBandSlot)
    Func<bool(const String&, void*)> onSlotRecv;
    Func<void(void*)> onSlotUpdate;
    Func<void(void*)> onSlotDetached;

    Array<void*> slots;
    bool isDynamicallyAllocated = false;

    Band();
    virtual ~Band();

    bool couldPush() const;
    void stun(u64 duration = 0);
    void push(const String &bundle);
    void recv(const String &bundle);
    void addSlot(void *slot);
    void removeSlot(void *slot);
    void update(bool doSlotUpdates = true);
};

} // namespace Comms

using Packet = Comms::Packet;
using Band = Comms::Band;

} // namespace Resource

namespace Comms {
    using Packet = Resource::Comms::Packet;
    using Band = Resource::Comms::Band;
}

using Packet = Resource::Comms::Packet;
using Band = Resource::Comms::Band;

} // namespace Ksee

#endif // KSEE_RESOURCE_COMMS_HPP
