/**
 * @file Path.cpp
 * @brief High-performance pure hierarchical Path extending directly and only Array<String>.
 */

#include "../../include/Ksee/Path.hpp"

namespace Ksee {

static bool isAllDigits(const String &s) {
    if (s.isEmpty()) return false;
    for (usz i = 0; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    return true;
}

static Array<String> splitByChar(const String &s, char delim) {
    Array<String> out;
    usz start = 0;
    for (usz i = 0; i < s.size(); ++i) {
        if (s[i] == (u8)delim) {
            out.push(s.substring(start, i));
            start = i + 1;
        }
    }
    out.push(s.substring(start));
    return out;
}

static bool parseIpv4String(const String &s, Array<String> &octets, String &port) {
    String host = s;
    port = "";
    long long colon = s.find(":");
    if (colon != -1) {
        host = s.substring(0, (usz)colon);
        port = s.substring((usz)colon + 1);
    }
    Array<String> dots = splitByChar(host, '.');
    if (dots.size() != 4) return false;
    for (usz i = 0; i < 4; ++i) {
        if (!isAllDigits(dots[i])) return false;
        long long v = dots[i].toLong();
        if (v < 0 || v > 255) return false;
    }
    octets = dots;
    return true;
}

static bool parseIpv6String(const String &s, Array<String> &hextets, String &port) {
    String host = s;
    port = "";
    if (s.startsWith("[") && s.includes("]")) {
        long long close = s.find("]");
        host = s.substring(1, (usz)close);
        if ((usz)close + 1 < s.size() && s[(usz)close + 1] == ':') {
            port = s.substring((usz)close + 2);
        }
    }
    if (!host.includes(":")) return false;
    Array<String> parts = splitByChar(host, ':');
    if (parts.size() < 3 || parts.size() > 8) return false;
    hextets = parts;
    return true;
}

// -----------------------------------------------------------------------------
// Constructors
// -----------------------------------------------------------------------------

Path::Path() : Array<String>() {}

Path::Path(const Array<String> &other) : Array<String>(other) {}

Path::Path(Array<String> &&other) noexcept : Array<String>(Ksee::Move(other)) {}

Path::Path(const char *str) : Path(String(str)) {}

Path::Path(const Array<u32> &parts) : Array<String>() {
    for (usz i = 0; i < parts.size(); ++i) {
        push(String::fromNumber(parts[i]));
    }
}

#if defined(KSEE_HAS_STD_INITIALIZER_LIST)
Path::Path(std::initializer_list<String> list) : Array<String>() {
    for (const auto &item : list) {
        push(item);
    }
}

Path::Path(std::initializer_list<const char*> list) : Array<String>() {
    for (const auto &item : list) {
        push(String(item));
    }
}
#endif

Path::Path(const String &path) : Array<String>() {
    if (path.isEmpty()) return;

    // 1. Direct comma-separated segments: "4,1,1,1,1,123" or "1,1" or "0,path,to"
    if (path.includes(",")) {
        Array<String> items = splitByChar(path, ',');
        for (usz i = 0; i < items.size(); ++i) {
            push(items[i]);
        }
        return;
    }

    // 2. IPv4 detection: "192.168.1.1:80" or "192.168.1.1"
    Array<String> octets;
    String portStr;
    if (parseIpv4String(path, octets, portStr)) {
        push("4");
        for (usz i = 0; i < 4; ++i) push(octets[i]);
        if (!portStr.isEmpty()) push(portStr);
        return;
    }

    // 3. IPv6 detection: "[::1]:8080" or "fe80::1"
    Array<String> hextets;
    if (parseIpv6String(path, hextets, portStr)) {
        push("6");
        for (usz i = 0; i < hextets.size(); ++i) push(hextets[i]);
        if (!portStr.isEmpty()) push(portStr);
        return;
    }

    // 4. Root path: starts with /
    if (path.startsWith("/")) {
        push("0");
        String rest = path.substring(1);
        if (!rest.isEmpty()) {
            Array<String> segs = splitByChar(rest, '/');
            for (usz i = 0; i < segs.size(); ++i) {
                push(segs[i]);
            }
        }
        return;
    }

    // 5. General relative path: "a/b/c"
    if (path.includes("/")) {
        Array<String> segs = splitByChar(path, '/');
        for (usz i = 0; i < segs.size(); ++i) {
            push(segs[i]);
        }
        return;
    }

    // Single token: "0", "1", "4", "6", "token"
    push(path);
}

Path &Path::operator=(const String &str) {
    *this = Path(str);
    return *this;
}

Path &Path::operator=(const char *str) {
    *this = Path(String(str));
    return *this;
}

// -----------------------------------------------------------------------------
// Static Constants
// -----------------------------------------------------------------------------

const Path Path::Back = Path(String("1,1"));
const Path Path::Control = Path(String("1"));
const Path Path::Self = Path(String("0"));
const Path Path::IPv4 = Path(String("4"));
const Path Path::IPv6 = Path(String("6"));
const Path Path::Ipv4 = Path::IPv4;
const Path Path::Ipv6 = Path::IPv6;

// -----------------------------------------------------------------------------
// Type / Mode checks
// -----------------------------------------------------------------------------

bool Path::isControl() const {
    return size() >= 1 && (*this)[0] == "1";
}

bool Path::isBack() const {
    return size() >= 2 && (*this)[0] == "1" && (*this)[1] == "1";
}

bool Path::isSelf() const {
    return size() == 1 && (*this)[0] == "0";
}

bool Path::isIPv4() const {
    return size() >= 1 && (*this)[0] == "4";
}

bool Path::isIPv6() const {
    return size() >= 1 && (*this)[0] == "6";
}

bool Path::isRoot() const {
    return size() >= 1 && (*this)[0] == "0";
}

// -----------------------------------------------------------------------------
// Network getters
// -----------------------------------------------------------------------------

Array<u8> Path::ipv4() const {
    Array<u8> res;
    if (isIPv4() && size() >= 5) {
        for (usz i = 1; i <= 4; ++i) {
            res.push((u8)(*this)[i].toLong());
        }
    }
    return res;
}

Array<u8> Path::ipv6() const {
    Array<u8> res;
    if (isIPv6() && size() > 1) {
        for (usz i = 1; i < size(); ++i) {
            if (isAllDigits((*this)[i])) {
                res.push((u8)(*this)[i].toLong());
            }
        }
    }
    return res;
}

u16 Path::port() const {
    if (isIPv4()) {
        if (size() >= 6 && isAllDigits((*this)[5])) {
            long long p = (*this)[5].toLong();
            if (p > 0 && p <= 65535) return (u16)p;
        }
        return 0;
    }
    if (isIPv6()) {
        if (size() > 2) {
            const String &last = (*this)[size() - 1];
            if (isAllDigits(last)) {
                long long p = last.toLong();
                if (p > 0 && p <= 65535) return (u16)p;
            }
        }
        return 0;
    }
    return 0;
}

// -----------------------------------------------------------------------------
// Common path computation with wildcard support (* and **)
// -----------------------------------------------------------------------------

static bool matchFull(const Path &a, usz i, const Path &b, usz j, Path &out) {
    if (i == a.size() && j == b.size()) return true;

    // a[i] == "**"
    if (i < a.size() && a[i] == "**") {
        // Try lazy matching: 0, 1, 2, ... items from b
        for (usz k = 0; j + k <= b.size(); ++k) {
            Path branch = out;
            for (usz m = 0; m < k; ++m) branch.push(b[j + m]);
            if (matchFull(a, i + 1, b, j + k, branch)) {
                out = branch;
                return true;
            }
        }
        return false;
    }

    // b[j] == "**"
    if (j < b.size() && b[j] == "**") {
        for (usz k = 0; i + k <= a.size(); ++k) {
            Path branch = out;
            for (usz m = 0; m < k; ++m) branch.push(a[i + m]);
            if (matchFull(a, i + k, b, j + 1, branch)) {
                out = branch;
                return true;
            }
        }
        return false;
    }

    if (i < a.size() && j < b.size()) {
        if (a[i] == "*" && b[j] == "*") {
            out.push("*");
            return matchFull(a, i + 1, b, j + 1, out);
        } else if (a[i] == "*") {
            out.push(b[j]);
            return matchFull(a, i + 1, b, j + 1, out);
        } else if (b[j] == "*") {
            out.push(a[i]);
            return matchFull(a, i + 1, b, j + 1, out);
        } else if (a[i] == b[j]) {
            out.push(a[i]);
            return matchFull(a, i + 1, b, j + 1, out);
        }
    }

    return false;
}

Path Path::common(const Path &other) const {
    // 1. Try full pattern match first (handles *, **)
    Path fullMatch;
    if (matchFull(*this, 0, other, 0, fullMatch)) {
        return fullMatch;
    }

    // 2. Otherwise return longest common prefix up to divergence
    Path lcp;
    usz i = 0, j = 0;
    while (i < size() && j < other.size()) {
        if ((*this)[i] == "*" && other[j] == "*") {
            lcp.push("*");
            i++; j++;
        } else if ((*this)[i] == "*") {
            lcp.push(other[j]);
            i++; j++;
        } else if (other[j] == "*") {
            lcp.push((*this)[i]);
            i++; j++;
        } else if ((*this)[i] == other[j]) {
            lcp.push((*this)[i]);
            i++; j++;
        } else {
            break;
        }
    }
    return lcp;
}

// -----------------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------------

String Path::toString() const {
    if (isEmpty()) return "";

    if (isIPv4() && size() >= 5) {
        String s = (*this)[1] + "." + (*this)[2] + "." + (*this)[3] + "." + (*this)[4];
        if (size() >= 6) {
            s += ":" + (*this)[5];
        }
        return s;
    }

    if (isIPv6() && size() > 1) {
        String s;
        usz lastIdx = (size() > 2 && isAllDigits((*this)[size() - 1])) ? size() - 1 : size();
        for (usz i = 1; i < lastIdx; ++i) {
            if (i > 1) s += ":";
            s += (*this)[i];
        }
        if (lastIdx < size()) {
            s = "[" + s + "]:" + (*this)[size() - 1];
        }
        return s;
    }

    if (isControl()) {
        String s;
        for (usz i = 0; i < size(); ++i) {
            if (i > 0) s += ",";
            s += (*this)[i];
        }
        return s;
    }

    if (isRoot()) {
        if (size() == 1) return "/";
        String s;
        for (usz i = 1; i < size(); ++i) {
            s += "/" + (*this)[i];
        }
        return s;
    }

    String s;
    for (usz i = 0; i < size(); ++i) {
        if (i > 0) s += "/";
        s += (*this)[i];
    }
    return s;
}

} // namespace Ksee
