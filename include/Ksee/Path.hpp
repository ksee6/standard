/**
 * @file Path.hpp
 * @brief Unified compact Path representation for Ksee.
 * Handles Root paths, Tagged paths, URLs, IPv4, IPv6, domain names, and pattern matching.
 */

#ifndef KSEE_PATH_HPP
#define KSEE_PATH_HPP

#include "Types.hpp"
#include "Array.hpp"
#include "String.hpp"

namespace Ksee {

class Path {
public:
    Array<String> parts;
    u8 mode = 0; // 0 = Relative, 1 = Root, 2 = Tagged, 3 = Url, 4 = Ipv4, 5 = Ipv6

    // Constructors
    Path();
    Path(const String &path);
    Path(const char *path);
    Path(const Array<String> &pathParts);
    Path(const Array<u32> &pathParts);

    // Indexed access
    String operator[](usz idx) const;
    usz size() const { return parts.size(); }
    bool isEmpty() const { return parts.size() == 0; }
    void clear() { parts.clear(); mode = 0; }

    // Mode checking
    bool isRoot() const;
    bool isTagged() const;
    bool isUrl() const;
    bool isIpv4() const;
    bool isIpv6() const;
    bool isBack() const;

    static const Path Back;

    // URL & Tag manipulation
    String getParam(const String &key) const;
    void setParam(const String &key, const String &value);

    Array<String> getTags() const;
    void setTag(const String &tag);
    bool hasTag(const String &tag) const;
    void removeTag(const String &tag);
    void removeTags();

    String getSchema() const;
    void setSchema(const String &schema);

    String getMethod() const;
    void setMethod(const String &method);

    // Network helpers
    Array<u8> ipv4() const;
    Array<u8> ipv6() const;
    u16 port() const;

    // Sub-path extraction
    Path base() const;
    Path path() const;

    // Comparison and Pattern Matching
    bool common(const Path &other) const;
    Array<u32> match(const Path &other) const;

    String toString() const;

    bool operator==(const Path &other) const;
    bool operator!=(const Path &other) const { return !(*this == other); }
};

} // namespace Ksee

#endif // KSEE_PATH_HPP

