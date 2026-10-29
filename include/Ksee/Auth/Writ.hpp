/**
 * @file Writ.hpp
 * @brief Trust token and capability delegation tree (Writ) for Ksee.
 */

#ifndef KSEE_AUTH_WRIT_HPP
#define KSEE_AUTH_WRIT_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "../Array.hpp"
#include "../Sparse.hpp"
#include "../Tree.hpp"
#include "../Crypto/ECC.hpp"
#include "../Crypto/Hash.hpp"

namespace Ksee {

class Writ {
public:
    /**
     * @brief Computes a deterministic 16-byte BLAKE3 hash of the writ.
     * Strips any 'vouch' and 'hash' fields and deterministically sorts all keys.
     * If a secret is provided, it is appended before hashing.
     */
    static String canonicalHash(const Tree<void> &writ, const String &secret = "");

    /**
     * @brief Adds a vouch (8B public key prefix + 64B Ed25519 signature) to the writ.
     * Signs the deterministic canonical bytes of the tree without vouches.
     */
    static void vouch(const EdKeypair &us, Tree<void> &writ);

    /**
     * @brief Removes vouches signed by 'us' from the writ.
     */
    static void unvouch(const EdKeypair &us, Tree<void> &writ);

    /**
     * @brief Removes all vouches from the writ.
     */
    static void unvouch(Tree<void> &writ);

    /**
     * @brief Computes and adds a 16-byte BLAKE3 symmetric hash using the private key.
     */
    static void hash(const EdKeypair &us, Tree<void> &writ);
    static void hash(const String &secret, Tree<void> &writ);

    /**
     * @brief Removes the hash field from the writ.
     */
    static void unhash(const EdKeypair &us, Tree<void> &writ);
    static void unhash(const String &secret, Tree<void> &writ);
    static void unhash(Tree<void> &writ);

    /**
     * @brief Validates writs and builds all possible chains of trust.
     * @param leafPublicHints Optional public keys / hints identifying leaf writs.
     * @param allWrits Pool of all writs.
     * @param roots Trusted roots (can be 32B public keys, private keys, or secrets).
     * @param now Current epoch ms. If 0, 'starts' and 'expires' are ignored.
     * @return Sparse array of chains of trust (each chain is a Sparse<Tree<void>> ordered root to leaf).
     */
    static Sparse<Sparse<Tree<void>>> validate(
        const Array<String> &leafPublicHints,
        const Array<Tree<void>> &allWrits,
        const Array<String> &roots,
        u64 now = 0
    );

    /**
     * @brief Merges and masks permission rules across a chain of trust.
     * Parents mask first; children cannot elevate permissions beyond parent constraints.
     * Returns an effective permission Tree that can be queried with query("path").
     */
    static Tree<void> mask(const Sparse<Tree<void>> &chain);
};

} // namespace Ksee

#endif // KSEE_AUTH_WRIT_HPP
