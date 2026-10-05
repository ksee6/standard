/**
 * @file SHA.hpp
 * @brief Standalone SHA-256 and SHA-512 cryptographic hash implementations for Ksee.
 */

#ifndef KSEE_CRYPTO_SHA_HPP
#define KSEE_CRYPTO_SHA_HPP

#include "../Types.hpp"
#include "../String.hpp"

namespace Ksee {

class SHA256 {
public:
    static constexpr usz BlockSize = 64;
    static constexpr usz DigestSize = 32;

    SHA256();
    void update(const void* data, usz len);
    void update(const String& str);
    void final(u8 digest[DigestSize]);
    String final();

    static String hash(const String& input);
    static String hash(const void* data, usz len);
    static void hash(const void* data, usz len, u8 digest[DigestSize]);

private:
    u32 state[8];
    u64 count;
    u8 buffer[BlockSize];

    void transform(const u8 block[BlockSize]);
    static const u32 K[64];
};

class SHA512 {
public:
    static constexpr usz BlockSize = 128;
    static constexpr usz DigestSize = 64;

    SHA512();
    void update(const void* data, usz len);
    void update(const String& str);
    void final(u8 digest[DigestSize]);
    String final();

    static String hash(const String& input);
    static String hash(const void* data, usz len);
    static void hash(const void* data, usz len, u8 digest[DigestSize]);

private:
    u64 state[8];
    u64 count[2];
    u8 buffer[BlockSize];

    void transform(const u8 block[BlockSize]);
    static const u64 K[80];
};

inline String hashSha256(const String& input) {
    return SHA256::hash(input);
}

inline String hashSha256(const void* data, usz len) {
    return SHA256::hash(data, len);
}

inline String hashSha512(const String& input) {
    return SHA512::hash(input);
}

inline String hashSha512(const void* data, usz len) {
    return SHA512::hash(data, len);
}

} // namespace Ksee

#endif // KSEE_CRYPTO_SHA_HPP

