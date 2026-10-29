/**
 * @file Path.cpp
 * @brief High-performance unified Path implementation for Ksee.
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

Path::Path() {}

Path::Path(const char *path) : Path(String(path)) {}

Path::Path(const Array<String> &pathParts) : parts(pathParts) {
    if (parts.size() > 0) {
        if (parts[0] == "0") mode = 1;
        else if (parts[0] == "1" && parts.size() >= 3 && isAllDigits(parts[1])) mode = 2;
        else if (parts[0] == "2" && parts.size() >= 8) mode = 3;
        else if (parts[0] == "3" && parts.size() >= 5) mode = 4;
        else if (parts[0] == "4" && parts.size() > 3) mode = 5;
        else mode = 0;
    }
}

Path::Path(const Array<u32> &pathParts) {
    for (usz i = 0; i < pathParts.size(); ++i) {
        parts.push(String::fromNumber(pathParts[i]));
    }
    mode = 0;
}

Path::Path(const String &path) {
    if (path.isEmpty()) {
        return;
    }

    // 1. Direct comma-separated list
    // e.g. "0,path,to" or "1,2,base,tag" or "2,schema,,4,..."
    if (path.includes(",") && (path.startsWith("0,") || path.startsWith("1,") || path.startsWith("2,") || path.startsWith("3,") || path.startsWith("4,"))) {
        parts = splitByChar(path, ',');
        if (parts.size() > 0) {
            if (parts[0] == "0") mode = 1;
            else if (parts[0] == "1" && parts.size() >= 3 && isAllDigits(parts[1])) mode = 2;
            else if (parts[0] == "2" && parts.size() >= 8) mode = 3;
            else if (parts[0] == "3" && parts.size() >= 5) mode = 4;
            else if (parts[0] == "4" && parts.size() > 3) mode = 5;
            else mode = 0;
        }
        return;
    }

    // 2. URL parsing: schema://[method@]host[:port]/path?query#tag
    long long schemaIdx = path.find("://");
    if (schemaIdx != -1) {
        String schema = path.substring(0, (usz)schemaIdx);
        String rest = path.substring((usz)schemaIdx + 3);

        // Strip extra slashes if e.g. schema:///host
        while (rest.startsWith("/")) {
            rest = rest.substring(1);
        }

        // Tag: #tag
        String tag = "";
        long long hashIdx = rest.find("#");
        if (hashIdx != -1) {
            tag = rest.substring((usz)hashIdx + 1);
            rest = rest.substring(0, (usz)hashIdx);
        }

        // Query: ?k1=v1&k2=v2
        Array<String> params;
        long long queryIdx = rest.find("?");
        if (queryIdx != -1) {
            String qStr = rest.substring((usz)queryIdx + 1);
            rest = rest.substring(0, (usz)queryIdx);
            Array<String> pairs = splitByChar(qStr, '&');
            for (usz i = 0; i < pairs.size(); ++i) {
                if (pairs[i].isEmpty()) continue;
                long long eq = pairs[i].find("=");
                if (eq != -1) {
                    params.push(pairs[i].substring(0, (usz)eq));
                    params.push(pairs[i].substring((usz)eq + 1));
                } else {
                    params.push(pairs[i]);
                    params.push("");
                }
            }
        }

        // Split host from path
        String hostPart = rest;
        String pathPart = "";
        long long slashIdx = rest.find("/");
        if (slashIdx != -1) {
            hostPart = rest.substring(0, (usz)slashIdx);
            pathPart = rest.substring((usz)slashIdx + 1);
        }

        // Method: method@host
        String method = "";
        long long atIdx = hostPart.find("@");
        if (atIdx != -1) {
            method = hostPart.substring(0, (usz)atIdx);
            hostPart = hostPart.substring((usz)atIdx + 1);
        }

        // Base path from hostPart
        Path basePath(hostPart);

        // Path segments
        Array<String> subPaths;
        if (!pathPart.isEmpty()) {
            Array<String> rawPaths = splitByChar(pathPart, '/');
            for (usz i = 0; i < rawPaths.size(); ++i) {
                if (rawPaths[i] == "*") {
                    subPaths.push("");
                } else {
                    subPaths.push(rawPaths[i]);
                }
            }
        }

        // Construct URL parts:
        // 2, schema, method, baseLength, (...base), pathLength, (...path), tag, paramKey1, paramVal1...
        mode = 3;
        parts.push("2");
        parts.push(schema);
        parts.push(method);
        parts.push(String::fromNumber(basePath.parts.size()));
        for (usz i = 0; i < basePath.parts.size(); ++i) {
            parts.push(basePath.parts[i]);
        }
        parts.push(String::fromNumber(subPaths.size()));
        for (usz i = 0; i < subPaths.size(); ++i) {
            parts.push(subPaths[i]);
        }
        parts.push(tag);
        for (usz i = 0; i < params.size(); ++i) {
            parts.push(params[i]);
        }
        return;
    }

    // 3. Tagged path: /path/to/something#tag1#tag2
    if (path.includes("#")) {
        Array<String> hashes = splitByChar(path, '#');
        String mainPath = hashes[0];
        Path baseP(mainPath);
        mode = 2;
        parts.push("1");
        parts.push(String::fromNumber(baseP.parts.size()));
        for (usz i = 0; i < baseP.parts.size(); ++i) {
            parts.push(baseP.parts[i]);
        }
        for (usz i = 1; i < hashes.size(); ++i) {
            parts.push(hashes[i]);
        }
        return;
    }

    // 4. IPv4 detection
    Array<String> ipOctets;
    String ipPort;
    if (parseIpv4String(path, ipOctets, ipPort)) {
        mode = 4;
        parts.push("3");
        for (usz i = 0; i < 4; ++i) parts.push(ipOctets[i]);
        if (!ipPort.isEmpty()) parts.push(ipPort);
        return;
    }

    // 5. IPv6 detection
    Array<String> ip6Hextets;
    if (parseIpv6String(path, ip6Hextets, ipPort)) {
        mode = 5;
        parts.push("4");
        for (usz i = 0; i < ip6Hextets.size(); ++i) parts.push(ip6Hextets[i]);
        if (!ipPort.isEmpty()) parts.push(ipPort);
        return;
    }

    // 6. Root path: starts with /
    if (path.startsWith("/")) {
        mode = 1;
        parts.push("0");
        String p = path.substring(1);
        if (!p.isEmpty()) {
            Array<String> slashes = splitByChar(p, '/');
            for (usz i = 0; i < slashes.size(); ++i) {
                if (slashes[i] == "*") parts.push("");
                else parts.push(slashes[i]);
            }
        }
        return;
    }

    // 7. Host with port: e.g. domain.google.com:80 or 127.0.0.1:8080 (no slashes)
    if (!path.includes("/") && path.includes(":")) {
        long long colon = path.find(":");
        if (colon != -1) {
            String host = path.substring(0, (usz)colon);
            String port = path.substring((usz)colon + 1);
            if (isAllDigits(port) && !port.isEmpty()) {
                mode = 0;
                parts.push(host);
                parts.push(port);
                return;
            }
        }
    }

    // 8. General relative path: path/to/something
    mode = 0;
    Array<String> slashes = splitByChar(path, '/');
    for (usz i = 0; i < slashes.size(); ++i) {
        if (slashes[i] == "*") parts.push("");
        else parts.push(slashes[i]);
    }
}

String Path::operator[](usz idx) const {
    if (idx >= parts.size()) return "";
    return parts[idx];
}

bool Path::isRoot() const {
    return mode == 1;
}

bool Path::isTagged() const {
    return mode == 2;
}

bool Path::isUrl() const {
    return mode == 3;
}

bool Path::isIpv4() const {
    if (mode == 4) return true;
    if (isUrl() || isTagged()) {
        return base().isIpv4();
    }
    return false;
}

bool Path::isIpv6() const {
    if (mode == 5) return true;
    if (isUrl() || isTagged()) {
        return base().isIpv6();
    }
    return false;
}

bool Path::isBack() const {
    return parts.size() == 2 && parts[0] == "0" && parts[1] == "1";
}

static Path createBackPath() {
    Path p;
    p.parts.push("0");
    p.parts.push("1");
    p.mode = 1;
    return p;
}

const Path Path::Back = createBackPath();

Path Path::base() const {
    if (isTagged()) {
        if (parts.size() < 2) return Path();
        usz baseLen = (usz)parts[1].toLong();
        Path b;
        for (usz i = 0; i < baseLen && (2 + i) < parts.size(); ++i) {
            b.parts.push(parts[2 + i]);
        }
        if (b.parts.size() > 0) {
            if (b.parts[0] == "0") b.mode = 1;
            else if (b.parts[0] == "1" && b.parts.size() >= 3 && isAllDigits(b.parts[1])) b.mode = 2;
            else if (b.parts[0] == "2" && b.parts.size() >= 8) b.mode = 3;
            else if (b.parts[0] == "3" && b.parts.size() >= 5) b.mode = 4;
            else if (b.parts[0] == "4" && b.parts.size() > 3) b.mode = 5;
            else b.mode = 0;
        }
        return b;
    }
    if (isUrl()) {
        if (parts.size() < 4) return Path();
        usz baseLen = (usz)parts[3].toLong();
        Path b;
        for (usz i = 0; i < baseLen && (4 + i) < parts.size(); ++i) {
            b.parts.push(parts[4 + i]);
        }
        if (b.parts.size() > 0) {
            if (b.parts[0] == "0") b.mode = 1;
            else if (b.parts[0] == "1" && b.parts.size() >= 3 && isAllDigits(b.parts[1])) b.mode = 2;
            else if (b.parts[0] == "2" && b.parts.size() >= 8) b.mode = 3;
            else if (b.parts[0] == "3" && b.parts.size() >= 5) b.mode = 4;
            else if (b.parts[0] == "4" && b.parts.size() > 3) b.mode = 5;
            else b.mode = 0;
        }
        return b;
    }
    return *this;
}

Path Path::path() const {
    if (isUrl()) {
        if (parts.size() < 4) return *this;
        usz baseLen = (usz)parts[3].toLong();
        usz pathOffset = 4 + baseLen;
        if (pathOffset >= parts.size()) return Path();
        usz pathLen = (usz)parts[pathOffset].toLong();
        Path p;
        for (usz i = 0; i < pathLen && (pathOffset + 1 + i) < parts.size(); ++i) {
            p.parts.push(parts[pathOffset + 1 + i]);
        }
        return p;
    }
    return *this;
}

String Path::getParam(const String &key) const {
    if (!isUrl()) return "";
    usz baseLen = (usz)parts[3].toLong();
    usz pathOffset = 4 + baseLen;
    if (pathOffset >= parts.size()) return "";
    usz pathLen = (usz)parts[pathOffset].toLong();
    usz tagOffset = pathOffset + 1 + pathLen;
    usz paramOffset = tagOffset + 1; // after tag

    for (usz i = paramOffset; i + 1 < parts.size(); i += 2) {
        if (parts[i] == key) {
            return parts[i + 1];
        }
    }
    return "";
}

void Path::setParam(const String &key, const String &value) {
    if (!isUrl()) {
        // Convert ourselves into a URL base
        Path old = *this;
        parts.clear();
        parts.push("2");
        parts.push(""); // schema
        parts.push(""); // method
        parts.push(String::fromNumber(old.parts.size()));
        for (usz i = 0; i < old.parts.size(); ++i) parts.push(old.parts[i]);
        parts.push("0"); // path length 0
        parts.push("");  // tag
        mode = 3;
    }

    usz baseLen = (usz)parts[3].toLong();
    usz pathOffset = 4 + baseLen;
    usz pathLen = (usz)parts[pathOffset].toLong();
    usz tagOffset = pathOffset + 1 + pathLen;
    usz paramOffset = tagOffset + 1;

    for (usz i = paramOffset; i + 1 < parts.size(); i += 2) {
        if (parts[i] == key) {
            if (value.isEmpty()) {
                // remove param
                parts.remove(i);
                parts.remove(i);
            } else {
                parts[i + 1] = value;
            }
            return;
        }
    }

    if (!value.isEmpty()) {
        parts.push(key);
        parts.push(value);
    }
}

Array<String> Path::getTags() const {
    Array<String> tags;
    if (isUrl()) {
        usz baseLen = (usz)parts[3].toLong();
        usz pathOffset = 4 + baseLen;
        if (pathOffset < parts.size()) {
            usz pathLen = (usz)parts[pathOffset].toLong();
            usz tagOffset = pathOffset + 1 + pathLen;
            if (tagOffset < parts.size() && !parts[tagOffset].isEmpty()) {
                tags.push(parts[tagOffset]);
            }
        }
        return tags;
    }
    if (isTagged()) {
        if (parts.size() >= 2) {
            usz baseLen = (usz)parts[1].toLong();
            for (usz i = 2 + baseLen; i < parts.size(); ++i) {
                tags.push(parts[i]);
            }
        }
        return tags;
    }
    return tags;
}

void Path::setTag(const String &tag) {
    if (isUrl()) {
        usz baseLen = (usz)parts[3].toLong();
        usz pathOffset = 4 + baseLen;
        usz pathLen = (usz)parts[pathOffset].toLong();
        usz tagOffset = pathOffset + 1 + pathLen;
        if (tagOffset < parts.size()) {
            parts[tagOffset] = tag;
        }
        return;
    }
    if (!isTagged()) {
        Path old = *this;
        parts.clear();
        mode = 2;
        parts.push("1");
        parts.push(String::fromNumber(old.parts.size()));
        for (usz i = 0; i < old.parts.size(); ++i) parts.push(old.parts[i]);
    }
    parts.push(tag);
}

bool Path::hasTag(const String &tag) const {
    Array<String> tags = getTags();
    for (usz i = 0; i < tags.size(); ++i) {
        if (tags[i] == tag) return true;
    }
    return false;
}

void Path::removeTag(const String &tag) {
    if (isUrl()) {
        usz baseLen = (usz)parts[3].toLong();
        usz pathOffset = 4 + baseLen;
        usz pathLen = (usz)parts[pathOffset].toLong();
        usz tagOffset = pathOffset + 1 + pathLen;
        if (tagOffset < parts.size() && parts[tagOffset] == tag) {
            parts[tagOffset] = "";
        }
        return;
    }
    if (isTagged()) {
        usz baseLen = (usz)parts[1].toLong();
        for (usz i = 2 + baseLen; i < parts.size(); ++i) {
            if (parts[i] == tag) {
                parts.remove(i);
                return;
            }
        }
    }
}

void Path::removeTags() {
    if (isUrl()) {
        usz baseLen = (usz)parts[3].toLong();
        usz pathOffset = 4 + baseLen;
        usz pathLen = (usz)parts[pathOffset].toLong();
        usz tagOffset = pathOffset + 1 + pathLen;
        if (tagOffset < parts.size()) {
            parts[tagOffset] = "";
        }
        return;
    }
    if (isTagged()) {
        Path b = base();
        *this = b;
    }
}

String Path::getSchema() const {
    if (isUrl() && parts.size() > 1) return parts[1];
    return "";
}

void Path::setSchema(const String &schema) {
    if (!isUrl()) {
        setParam("", "");
    }
    if (parts.size() > 1) {
        parts[1] = schema;
    }
}

String Path::getMethod() const {
    if (isUrl() && parts.size() > 2) return parts[2];
    return "";
}

void Path::setMethod(const String &method) {
    if (!isUrl()) {
        setParam("", "");
    }
    if (parts.size() > 2) {
        parts[2] = method;
    }
}

Array<u8> Path::ipv4() const {
    Array<u8> res;
    Path b = isUrl() || isTagged() ? base() : *this;
    if (b.parts.size() >= 5 && b.parts[0] == "3") {
        for (usz i = 1; i <= 4; ++i) {
            res.push((u8)b.parts[i].toLong());
        }
    }
    return res;
}

Array<u8> Path::ipv6() const {
    Array<u8> res;
    Path b = isUrl() || isTagged() ? base() : *this;
    if (b.parts.size() > 1 && b.parts[0] == "4") {
        for (usz i = 1; i < b.parts.size(); ++i) {
            if (isAllDigits(b.parts[i])) {
                res.push((u8)b.parts[i].toLong());
            }
        }
    }
    return res;
}

u16 Path::port() const {
    Path b = isUrl() || isTagged() ? base() : *this;
    if (b.parts.size() >= 6 && b.parts[0] == "3") {
        return (u16)b.parts[5].toLong();
    }
    if (b.parts.size() > 0) {
        const String &last = b.parts[b.parts.size() - 1];
        if (isAllDigits(last)) {
            long long val = last.toLong();
            if (val > 0 && val <= 65535) {
                return (u16)val;
            }
        }
    }
    return 0;
}

bool Path::common(const Path &other) const {
    // When comparing URLs to non-URLs: compare path from URL to other as whole
    if (isUrl() && !other.isUrl()) {
        return path().common(other);
    }
    if (!isUrl() && other.isUrl()) {
        return common(other.path());
    }

    if (isUrl() && other.isUrl()) {
        // Compare base first
        if (!base().common(other.base())) return false;
        // Compare path
        if (!path().common(other.path())) return false;
        return true;
    }

    // Standard sequence comparison with wildcard support:
    // "" matches any single part
    // "*" matches any single part
    // "**" matches any number of parts
    usz i = 0, j = 0;
    while (i < parts.size() && j < other.parts.size()) {
        if (parts[i] == "**") {
            if (i + 1 == parts.size()) return true; // trailing ** matches rest
            // Lazy scan for parts[i+1]
            while (j < other.parts.size()) {
                Path sub1, sub2;
                for (usz k = i + 1; k < parts.size(); ++k) sub1.parts.push(parts[k]);
                for (usz k = j; k < other.parts.size(); ++k) sub2.parts.push(other.parts[k]);
                if (sub1.common(sub2)) return true;
                j++;
            }
            return false;
        }
        if (other.parts[j] == "**") {
            return other.common(*this);
        }

        if (parts[i].isEmpty() || parts[i] == "*" ||
            other.parts[j].isEmpty() || other.parts[j] == "*" ||
            parts[i] == other.parts[j]) {
            i++;
            j++;
        } else {
            return false;
        }
    }

    if (i < parts.size() && parts[i] == "**" && i + 1 == parts.size()) return true;
    if (j < other.parts.size() && other.parts[j] == "**" && j + 1 == other.parts.size()) return true;

    return i == parts.size() && j == other.parts.size();
}

Array<u32> Path::match(const Path &other) const {
    Array<u32> extracted;
    usz i = 0, j = 0;
    while (i < parts.size() && j < other.parts.size()) {
        if (parts[i] == "**") {
            if (i + 1 == parts.size()) break;
            while (j < other.parts.size()) {
                Path sub1, sub2;
                for (usz k = i + 1; k < parts.size(); ++k) sub1.parts.push(parts[k]);
                for (usz k = j; k < other.parts.size(); ++k) sub2.parts.push(other.parts[k]);
                if (sub1.common(sub2)) {
                    Array<u32> subM = sub1.match(sub2);
                    for (usz m = 0; m < subM.size(); ++m) extracted.push(subM[m]);
                    return extracted;
                }
                j++;
            }
            break;
        }
        if (parts[i] == "*" || parts[i].isEmpty()) {
            if (isAllDigits(other.parts[j])) {
                extracted.push((u32)other.parts[j].toLong());
            } else {
                extracted.push((u32)j);
            }
            i++;
            j++;
        } else {
            i++;
            j++;
        }
    }
    return extracted;
}

String Path::toString() const {
    if (parts.isEmpty()) return "";

    // 1. Root path
    if (isRoot()) {
        String s = "/";
        for (usz i = 1; i < parts.size(); ++i) {
            if (i > 1) s += "/";
            s += parts[i];
        }
        return s;
    }

    // 2. Tagged path
    if (isTagged()) {
        Path b = base();
        String s = b.toString();
        Array<String> tags = getTags();
        for (usz i = 0; i < tags.size(); ++i) {
            s += "#" + tags[i];
        }
        return s;
    }

    // 3. IPv4
    if (parts.size() >= 5 && parts[0] == "3") {
        String s = parts[1] + "." + parts[2] + "." + parts[3] + "." + parts[4];
        if (parts.size() >= 6) {
            s += ":" + parts[5];
        }
        return s;
    }

    // 4. IPv6
    if (parts.size() > 1 && parts[0] == "4") {
        String s;
        for (usz i = 1; i < parts.size(); ++i) {
            if (i > 1) s += ":";
            s += parts[i];
        }
        return s;
    }

    // 5. URL
    if (isUrl()) {
        String schema = getSchema();
        String method = getMethod();
        Path b = base();
        Path p = path();
        Array<String> tags = getTags();

        String s;
        if (!schema.isEmpty()) {
            s += schema + "://";
        }
        if (!method.isEmpty()) {
            s += method + "@";
        }
        s += b.toString();
        if (!p.isEmpty()) {
            s += "/" + p.toString();
        }

        // Params
        usz baseLen = (usz)parts[3].toLong();
        usz pathOffset = 4 + baseLen;
        usz pathLen = (usz)parts[pathOffset].toLong();
        usz tagOffset = pathOffset + 1 + pathLen;
        usz paramOffset = tagOffset + 1;

        bool firstParam = true;
        for (usz i = paramOffset; i + 1 < parts.size(); i += 2) {
            s += firstParam ? "?" : "&";
            firstParam = false;
            s += parts[i] + "=" + parts[i + 1];
        }

        if (tags.size() > 0 && !tags[0].isEmpty()) {
            s += "#" + tags[0];
        }
        return s;
    }

    // 6. Host with port or relative path
    if (parts.size() == 2 && isAllDigits(parts[1]) && !parts[1].isEmpty()) {
        long long p = parts[1].toLong();
        if (p > 0 && p <= 65535) {
            return parts[0] + ":" + parts[1];
        }
    }
    String s;
    for (usz i = 0; i < parts.size(); ++i) {
        if (i > 0) s += "/";
        s += parts[i];
    }
    return s;
}

bool Path::operator==(const Path &other) const {
    if (parts.size() != other.parts.size()) return false;
    for (usz i = 0; i < parts.size(); ++i) {
        if (parts[i] != other.parts[i]) return false;
    }
    return true;
}

} // namespace Ksee

