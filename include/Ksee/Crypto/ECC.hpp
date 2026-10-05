/**
 * @file ECC.hpp
 * @brief Unified Ed25519 and X25519 elliptic curve cryptography for Ksee.
 */

#ifndef KSEE_CRYPTO_ECC_HPP
#define KSEE_CRYPTO_ECC_HPP

#include "../Types.hpp"
#include "../String.hpp"

namespace Ksee {

class EdKeypair;
class XKeypair;

u256 edToXPrivateKey(const u256 &edSecretKey);
u256 edToXPublicKey(const u256 &edPublicKey);

inline u256 edToXPrivateKey(const String &edSecretKey) { return edToXPrivateKey(u256(edSecretKey)); }
inline u256 edToXPublicKey(const String &edPublicKey) { return edToXPublicKey(u256(edPublicKey)); }

class XKeypair {
private:
    mutable u256 _cachedPub;
    mutable bool _hasPub = false;

public:
    u256 secretKey;

    XKeypair();
    explicit XKeypair(const u256 &priv);
    explicit XKeypair(const String &priv);
    XKeypair(const EdKeypair &ed);
    XKeypair(const XKeypair &o);
    XKeypair(XKeypair &&o) noexcept;
    XKeypair &operator=(const XKeypair &o);
    XKeypair &operator=(XKeypair &&o) noexcept;

    const u256 &publicKey() const;

    static XKeypair generate();
    u256 sharedKey(const u256 &theirPublic) const;
    inline u256 sharedKey(const String &theirPublic) const { return sharedKey(u256(theirPublic)); }
};

class EdKeypair {
private:
    mutable u256 _cachedPub;
    mutable bool _hasPub = false;

public:
    u256 secretKey;

    EdKeypair();
    explicit EdKeypair(const u256 &priv);
    explicit EdKeypair(const String &priv);
    EdKeypair(const EdKeypair &o);
    EdKeypair(EdKeypair &&o) noexcept;
    EdKeypair &operator=(const EdKeypair &o);
    EdKeypair &operator=(EdKeypair &&o) noexcept;

    const u256 &publicKey() const;

    static EdKeypair generate();
    u256 sharedKey(const u256 &theirPublic) const;
    inline u256 sharedKey(const String &theirPublic) const { return sharedKey(u256(theirPublic)); }

    u512 sign(const String &message) const;
    bool verify(const String &message, const u512 &signature) const;
    inline bool verify(const String &message, const String &signature) const {
        if (signature.size() != 64) return false;
        return verify(message, u512((const u8*)signature.data()));
    }
};

u512 edSign(const u256 &secretKey, const String &message);
inline u512 edSign(const String &secretKey, const String &message) { return edSign(u256(secretKey), message); }

bool edVerify(const u256 &publicKey, const String &message, const u512 &signature);
inline bool edVerify(const String &publicKey, const String &message, const String &signature) {
    if (publicKey.size() != 32 || signature.size() != 64) return false;
    return edVerify(u256(publicKey), message, u512((const u8*)signature.data()));
}

} // namespace Ksee

#endif // KSEE_CRYPTO_ECC_HPP

