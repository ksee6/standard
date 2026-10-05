/**
 * @file JWT.hpp
 * @brief JSON Web Token (JWT) HS256 sealing and verification for Ksee.
 */

#ifndef KSEE_FORMAT_JWT_HPP
#define KSEE_FORMAT_JWT_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "../Tree.hpp"

namespace Ksee {

class JWT {
public:
    static String seal(const Tree<void> &tree, const String &key = "");
    static String seal(const Tree<void> *tree, const String &key = "");
    static String seal(const String &jsonPayload, const String &key = "");

    static Tree<void> open(const String &token, const String &key = "");
    static bool verify(const String &token, const String &key = "");
};

} // namespace Ksee

#endif // KSEE_FORMAT_JWT_HPP

