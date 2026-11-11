/**
 * @file Comms.cpp
 * @brief Implementation of Standard Band physical medium.
 */

#include <Ksee/Resource/Comms.hpp>

namespace Ksee {
namespace Resource {
namespace Comms {

Band::Band() {
    lastDebtRegenTime = millis();
}

Band::~Band() {
    for (usz i = 0; i < slots.size(); ++i) {
        if (slots[i] && onSlotDetached) {
            onSlotDetached(slots[i]);
        }
    }
    slots.clear();
}

bool Band::couldPush() const {
    if (millis() < stunnedUntil) return false;
    if (maxDuty > 0 && duty >= maxDuty) return false;
    return true;
}

void Band::stun(u64 duration) {
    if (duration == 0) {
        u64 range = (maxStunTime > minStunTime) ? (maxStunTime - minStunTime + 1) : 1;
        duration = minStunTime + (randomNumber<u32>() % (u32)range);
    }
    u64 until = millis() + duration;
    if (until > stunnedUntil) stunnedUntil = until;
}

void Band::push(const String &bundle) {
    if (!couldPush()) return;
    if (maxBundleSize > 0 && bundle.size() > maxBundleSize) return;

    u64 a = millis();
    if (sendCallback) {
        sendCallback(bundle);
    }
    u64 b = millis();
    u64 airtime = (b >= a) ? (b - a) : 0;
    duty += (airtime > 0 ? airtime : 1);
}

void Band::recv(const String &bundle) {
    if (maxBundleSize > 0 && bundle.size() > maxBundleSize) return;
    if (onRecv) onRecv(bundle);

    for (usz i = 0; i < slots.size(); ++i) {
        if (onSlotRecv) {
            if (onSlotRecv(bundle, slots[i])) {
                break;
            }
        }
    }
}

void Band::addSlot(void *slot) {
    if (!slot) return;
    for (usz i = 0; i < slots.size(); ++i) {
        if (slots[i] == slot) return;
    }
    slots.push(slot);
}

void Band::removeSlot(void *slot) {
    for (usz i = 0; i < slots.size(); ++i) {
        if (slots[i] == slot) {
            slots.remove(i);
            break;
        }
    }
    if (slot && onSlotDetached) {
        onSlotDetached(slot);
    }
    if (slots.isEmpty() && isDynamicallyAllocated) {
        delete this;
    }
}

void Band::update(bool doSlotUpdates) {
    u64 now = millis();
    if (lastDebtRegenTime == 0) lastDebtRegenTime = now;
    if (now >= lastDebtRegenTime + 1000) {
        u64 elapsedSec = (now - lastDebtRegenTime) / 1000;
        lastDebtRegenTime += elapsedSec * 1000;
        if (maxDuty > 0) {
            u64 deduct = maxDuty * elapsedSec;
            if (duty > deduct) duty -= deduct;
            else duty = 0;
        } else {
            duty = 0;
        }
    }

    if (recvCallback) {
        String data = recvCallback();
        if (data.size() > 0) {
            recv(data);
        }
    }

    if (doSlotUpdates && onSlotUpdate) {
        for (usz i = 0; i < slots.size(); ++i) {
            if (slots[i]) {
                onSlotUpdate(slots[i]);
            }
        }
    }
}

} // namespace Comms
} // namespace Resource
} // namespace Ksee
