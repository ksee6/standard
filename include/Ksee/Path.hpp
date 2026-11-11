/**
 * @file Path.hpp
 * @brief Pure hierarchical Path extending directly and only Array<String>.
 */

#ifndef KSEE_PATH_HPP
#define KSEE_PATH_HPP

#include "Ksee.hpp"
#include "Array.hpp"
#include "String.hpp"

namespace Ksee {

class Path : public Array<String> {
public:
    // Inherit constructors from Array<String>
    using Array<String>::Array;

    // Constructors
    Path();
    Path(const Path &other) = default;
    Path(Path &&other) noexcept = default;
    Path(const String &str);
    Path(const char *str);
    Path(const Array<String> &other);
    Path(Array<String> &&other) noexcept;
    Path(const Array<u32> &parts);
#if defined(KSEE_HAS_STD_INITIALIZER_LIST)
    Path(std::initializer_list<String> list);
    Path(std::initializer_list<const char*> list);
#endif

    Path &operator=(const Path &other) = default;
    Path &operator=(Path &&other) noexcept = default;
    Path &operator=(const String &str);
    Path &operator=(const char *str);

    // Static constants
    static const Path Back;
    static const Path Control;
    static const Path Self;
    static const Path IPv4;
    static const Path IPv6;
    static const Path Ipv4;
    static const Path Ipv6;

    // Type / Mode checks
    bool isControl() const;
    bool isBack() const;
    bool isSelf() const;
    bool isIPv4() const;
    bool isIpv4() const { return isIPv4(); }
    bool isIPv6() const;
    bool isIpv6() const { return isIPv6(); }
    bool isRoot() const;

    // Network getters
    Array<u8> ipv4() const;
    Array<u8> ipv6() const;
    u16 port() const;

    // Common path computation with wildcard support (* and **)
    Path common(const Path &other) const;

    // Serialization
    String toString() const;
};

} // namespace Ksee

#endif // KSEE_PATH_HPP
