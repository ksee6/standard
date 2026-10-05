/**
 * @file Hash.hpp
 * @brief High-performance standalone BLAKE3 cryptographic hash and KDF for Ksee.
 */

#ifndef KSEE_CRYPTO_HASH_HPP
#define KSEE_CRYPTO_HASH_HPP

#include "../Types.hpp"
#include "../String.hpp"

namespace Ksee {

void hash(const char *input, usz length, usz outputPtr, usz outputLength = 32);

inline void hash(char *input, usz length, usz outputPtr, usz outputLength = 32) {
    hash((const char *)input, length, outputPtr, outputLength);
}

inline void hash(const char *input, usz length, void *outputPtr, usz outputLength = 32) {
    hash(input, length, (usz)outputPtr, outputLength);
}

inline void hash(const char *input, usz length, u8 *outputPtr, usz outputLength = 32) {
    hash(input, length, (usz)outputPtr, outputLength);
}

String hash(const String &input, usz outputLength = 32);

String kdf(const String &input, u64 salt, const String &domain, usz outputLength = 32);

} // namespace Ksee

#endif // KSEE_CRYPTO_HASH_HPP

