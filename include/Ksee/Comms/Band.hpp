/**
 * @file Band.hpp
 * @brief Standard Band physical medium with duty cycle management and slot dispatch.
 */

#ifndef KSEE_COMMS_BAND_HPP
#define KSEE_COMMS_BAND_HPP

#include "../Ksee.hpp"
#include "../String.hpp"
#include "../Array.hpp"
#include "../Func.hpp"
#include "../Math/Random.hpp"

namespace Ksee {
namespace Comms {

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
} // namespace Ksee

#endif // KSEE_COMMS_BAND_HPP
