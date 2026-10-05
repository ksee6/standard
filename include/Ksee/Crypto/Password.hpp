/**
 * @file Password.hpp
 * @brief Standalone Argon2id password-based key derivation function (PBKDF) for Ksee.
 */

#ifndef KSEE_CRYPTO_PASSWORD_HPP
#define KSEE_CRYPTO_PASSWORD_HPP

#include "../Types.hpp"
#include "../String.hpp"

namespace Ksee {

String pbkdf(const String &input, u64 salt, const String &domain,
                         usz outputLength = 32, u32 kilobytes = 19, u32 time = 2);

} // namespace Ksee

#endif // KSEE_CRYPTO_PASSWORD_HPP

