/**
 * @file AEAD.hpp
 * @brief RFC 8439 ChaCha12-Poly1305 Authenticated Encryption with Associated Data for Ksee.
 */

#ifndef KSEE_CRYPTO_AEAD_HPP
#define KSEE_CRYPTO_AEAD_HPP

#include "../Types.hpp"
#include "../String.hpp"

namespace Ksee {

struct AEAD {
    String text;
    String header;
    String hash;
    bool ok = true;

    AEAD() = default;
    AEAD(const String &str, usz headerLength = 1);

    static AEAD fromStringVarHeader(const String &str);

    String toString(usz headerLength = 1) const;
    String toStringVarHeader() const;
};

void poly1305_auth(u8 tag[16], const u8 *msg, usz len, const u8 key[32]);

AEAD seal(const AEAD &input, u256 key, u64 nonce, int rounds = 12);
AEAD open(const AEAD &input, u256 key, u64 nonce, int rounds = 12);

inline AEAD seal(const AEAD &input, const String &key, u64 nonce, int rounds = 12) {
    return seal(input, u256(key), nonce, rounds);
}

inline AEAD open(const AEAD &input, const String &key, u64 nonce, int rounds = 12) {
    return open(input, u256(key), nonce, rounds);
}

inline AEAD openSeal(const AEAD &input, u256 key, u64 nonce, int rounds = 12) {
    return open(input, key, nonce, rounds);
}

inline AEAD openSeal(const AEAD &input, const String &key, u64 nonce, int rounds = 12) {
    return open(input, u256(key), nonce, rounds);
}

} // namespace Ksee

#endif // KSEE_CRYPTO_AEAD_HPP

