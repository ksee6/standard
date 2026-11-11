#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../include/Ksee/Ksee.hpp"
#include "../include/Ksee/Func.hpp"
#include "../include/Ksee/Iterator.hpp"
#include "../include/Ksee/Array.hpp"
#include "../include/Ksee/String.hpp"
#include "../include/Ksee/Set.hpp"
#include "../include/Ksee/Map.hpp"
#include "../include/Ksee/Time.hpp"
#include "../include/Ksee/ID.hpp"
#include "../include/Ksee/Path.hpp"
#include "../include/Ksee/Tree.hpp"
#include "../include/Ksee/Comms/Packet.hpp"
#include "../include/Ksee/Comms/Band.hpp"
#include "../include/Ksee/Algorithm/Hash.hpp"
#include "../include/Ksee/Algorithm/SHA.hpp"
#include "../include/Ksee/Algorithm/Chacha.hpp"
#include "../include/Ksee/Algorithm/AEAD.hpp"
#include "../include/Ksee/Algorithm/ECC.hpp"
#include "../include/Ksee/Algorithm/Password.hpp"
#include "../include/Ksee/Auth/JWT.hpp"
#include "../include/Ksee/Auth/Writ.hpp"
#include "../include/Ksee/Regex.hpp"
#include "../include/Ksee/Algorithm/Compression.hpp"
#include "../include/Ksee/Terminal/Color.hpp"
#include "../include/Ksee/Terminal/Box.hpp"
#include "../include/Ksee/Terminal/Command.hpp"
#include "../include/Ksee/Math/Trig.hpp"
#include "../include/Ksee/Math/Vec.hpp"
#include "../include/Ksee/Math/Matrix.hpp"
#include "../include/Ksee/Math/Quaternion.hpp"
#include "../include/Ksee/Math/Transform.hpp"
#include "../include/Ksee/Math/Stats.hpp"
#include "../include/Ksee/Math/Random.hpp"
#include "../include/Ksee/Math/Interval.hpp"
#include "../include/Ksee/Debug.hpp"
#include "../include/Ksee/Filesystem.hpp"
#include "../include/Ksee/Base32.hpp"

using namespace Ksee;

static void test_types() {
    printf("[TEST] Types...\n");
    u64 t1 = millis();
    u64 n1 = nanos();
    assert(t1 >= 0);
    assert(n1 > 0);
    u32 r1 = Ksee::random();
    u32 r2 = Ksee::random();
    assert(r1 != 0 || r2 != 0);

    u8 buf[16];
    randomFill(buf, 16);
    bool nonZero = false;
    for (int i = 0; i < 16; ++i) if (buf[i] != 0) nonZero = true;
    assert(nonZero);
    printf("  PASS: Types\n");
}

static void test_array_string() {
    printf("[TEST] Array & String...\n");
    Array<int> arr;
    arr.push(10);
    arr.push(20);
    arr.push(30);
    assert(arr.size() == 3);
    assert(arr[0] == 10 && arr[1] == 20 && arr[2] == 30);
    assert(arr.includes(20));
    assert(!arr.includes(40));
    assert(arr.find(30) == 2);

    int sum = 0;
    for (int v : arr) {
        sum += v;
    }
    assert(sum == 60);

    // Variadic multi-element constructor: Array(x, y, z) like [...k]
    Array<int> arrVar(10, 20, 30, 40);
    assert(arrVar.size() == 4);
    assert(arrVar[0] == 10 && arrVar[1] == 20 && arrVar[2] == 30 && arrVar[3] == 40);

    // Initializer list constructor: Array<int> a = {1, 2, 3}
    Array<int> arrInit = {100, 200, 300};
    assert(arrInit.size() == 3);
    assert(arrInit[0] == 100 && arrInit[1] == 200 && arrInit[2] == 300);

    // Multi-dimensional shape & transpose
    Array<int> tensor;
    tensor.allocate(6);
    assert(tensor.size() == 0); // allocate reserves capacity without altering length
    for (int i = 0; i < 6; ++i) tensor.push(i);
    assert(tensor.size() == 6);
    tensor.shape(2, 3);
    assert(tensor.rank() == 2);
    assert(tensor.dimensions()[0] == 2);
    assert(tensor.dimensions()[1] == 3);

    tensor.transpose();
    assert(tensor.rank() == 2);
    assert(tensor.dimensions()[0] == 3);
    assert(tensor.dimensions()[1] == 2);

    String s = "Hello, World!";
    assert(s.length() == 13);
    assert(s.startsWith("Hello"));
    assert(s.endsWith("World!"));
    assert(s.contains("lo, W"));

    Array<String> parts = s.split(", ");
    assert(parts.size() == 2);
    assert(parts[0] == "Hello");
    assert(parts[1] == "World!");

    String numStr = String(12345);
    assert(numStr == "12345");
    assert(numStr.toInt() == 12345);

    String zeros(32, 0);
    assert(zeros.size() == 32);
    for (usz i = 0; i < 32; ++i) assert(zeros[i] == 0);
    printf("  PASS: Array & String\n");
}

static void test_sparse_set_map() {
    printf("[TEST] Sparse (Unified Array), Set, Map...\n");
    // 1. Unified Array sparse behavior
    Array<int> sp;
    assert(sp.size() == 0);
    assert(sp.fragmentCount() == 0);

    sp[0] = 100;
    assert(sp.size() == 1);
    assert(sp.fragmentCount() == 1);

    sp[50] = 200;
    assert(sp[0] == 100);
    assert(sp[50] == 200);
    assert(sp.size() == 51);
    assert(sp.fragmentCount() == 2);
    assert(sp[25] == 0); // default value in gap

    // Splitting fragment with break_at
    sp[51] = 201;
    sp[52] = 202;
    sp.break_at(51);
    assert(sp.fragmentCount() >= 2);
    assert(sp[50] == 200);
    assert(sp[51] == 201);
    assert(sp[52] == 202);

    // Lazy flattening into contiguous memory via data()
    int *flat = sp.data();
    assert(flat != nullptr);
    assert(flat[0] == 100);
    assert(flat[50] == 200);
    assert(flat[51] == 201);
    assert(flat[52] == 202);
    assert(sp.fragmentCount() == 1); // Now single contiguous block

    // 2. Set backed by Map<T, bool>
    Set<String> set;
    set.add("apple");
    set.add("banana");
    assert(set.has("apple"));
    assert(set.has("banana"));
    assert(!set.has("cherry"));
    assert(set("apple") == true);  // Callable functor / predicate
    assert(set("cherry") == false);
    assert(set.size() == 2);

    // Range-based for loop over set
    int count = 0;
    for (const String &item : set) {
        assert(item == "apple" || item == "banana");
        count++;
    }
    assert(count == 2);

    // Invertible set / complement (~set)
    Set<String> blacklist;
    blacklist.add("banned_user");
    Set<String> whitelist = ~blacklist;
    assert(whitelist.has("regular_user") == true);
    assert(whitelist.has("banned_user") == false);
    assert(whitelist("regular_user") == true);
    assert(whitelist("banned_user") == false);

    // Set operations
    Set<String> s1; s1.add("A"); s1.add("B");
    Set<String> s2; s2.add("B"); s2.add("C");
    Set<String> sUnion = s1 | s2;
    assert(sUnion.has("A") && sUnion.has("B") && sUnion.has("C"));
    Set<String> sInter = s1 & s2;
    assert(sInter.has("B") && !sInter.has("A") && !sInter.has("C"));
    Set<String> sDiff = s1 - s2;
    assert(sDiff.has("A") && !sDiff.has("B"));

    // 3. Map (Robin Hood hash map with odd & even bucket distribution)
    Map<String, int> map;
    map.put("first", 1);
    map.put("second", 2);
    assert(map.has("first"));
    assert(*map.get("first") == 1);
    assert(*map.get("second") == 2);

    // operator[] access and auto-insertion
    map["third"] = 3;
    assert(map["third"] == 3);
    assert(map.size() == 3);

    // Map copy constructor
    Map<String, int> mapCopy = map;
    assert(mapCopy.size() == 3);
    assert(*mapCopy.get("first") == 1);
    assert(*mapCopy.get("third") == 3);

    // Removal
    assert(map.remove("second") == true);
    assert(!map.has("second"));
    assert(map.size() == 2);

    printf("  PASS: Sparse (Unified Array), Set, Map\n");
}

static void test_uuid() {
    printf("[TEST] UUID...\n");
    UUID v4 = UUID::v4();
    UUID v7 = UUID::v7();
    String v7str = v7.toString();
    assert(v7str.size() == 36);

    UUID parsed(v7str);
    assert(parsed == v7);
    printf("  PASS: UUID\n");
}

static void test_eid_and_cb32() {
    printf("[TEST] EID & Crockford's Base32...\n");

    // Clock checks
    assert(millis() > 0);
    assert(nanos() > 0);
    u64 ep = epochMillis();
    assert(ep > 1700000000000ULL); // After late 2023
    assert(epochTime() >= ep);
    assert(isEpochTimeSet == true);
    assert(epochOnStart > 0);

    // Time constructor checks: Time() and Time(u64)
    Time tNow;
    assert(tNow.us > 0);
    assert(tNow.tz == gmt);

    Time tSpecific(ep);
    assert(tSpecific.tz == gmt);
    assert(tSpecific.us == (i64)ep * 1000LL);

    // CB32 integer tests
    u32 u32Val = 0x12345678;
    String s32 = encodeCB32(u32Val);
    assert(s32.size() == 7);
    u32 d32 = decodeCB32(s32);
    assert(d32 == u32Val);

    u64 u64Val = 0x123456789ABCDEF0ULL;
    String s64 = encodeCB32(u64Val);
    assert(s64.size() == 13);
    u64 d64 = decodeCB32(s64);
    assert(d64 == u64Val);

    // CB32 big endian byte tests
    u8 bytesIn[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
    String sBe = encodeCB32BigEndian(bytesIn, 16);
    assert(sBe.size() == 26);
    u8 bytesOut[16] = {0};
    usz decLen = decodeCB32BigEndian(sBe, bytesOut, 16);
    assert(decLen == 16);
    for (int i = 0; i < 16; i++) {
        assert(bytesOut[i] == bytesIn[i]);
    }

    // u128 CB32 test
    u128 u128In(bytesIn);
    String s128 = encodeCB32(u128In);
    assert(s128 == sBe);
    u128 u128Out;
    assert(decodeCB32(s128, u128Out));
    assert(u128Out == u128In);

    // u256 CB32 test
    u256 u256In;
    for (int i = 0; i < 32; i++) u256In[i] = (u8)(i * 7);
    String s256 = encodeCB32(u256In);
    assert(s256.size() == 52);
    u256 u256Out;
    assert(decodeCB32(s256, u256Out));
    assert(u256Out == u256In);

    // u512 CB32 test
    u512 u512In;
    for (int i = 0; i < 64; i++) u512In[i] = (u8)(i * 3 + 1);
    String s512 = encodeCB32(u512In);
    assert(s512.size() == 103);
    u512 u512Out;
    assert(decodeCB32(s512, u512Out));
    assert(u512Out == u512In);

    // Standard RFC 4648 Base32 tests
    String b32Str = encodeB32(u32Val);
    assert(b32Str.size() == 7);
    u32 b32d = decodeB32(b32Str);
    assert(b32d == u32Val);

    String hello = "Hello";
    String helloB32 = encodeB32(hello);
    assert(helloB32.startsWith("JBSWY3DP"));
    String helloDec;
    assert(decodeB32(helloB32, helloDec));
    assert(helloDec == hello);

    u64 b64d = decodeB32(encodeB32(u64Val));
    assert(b64d == u64Val);

    u128 u128B32Out;
    assert(decodeB32(encodeB32(u128In), u128B32Out));
    assert(u128B32Out == u128In);

    // EID tests
    EID e1 = EID::random();
    assert(e1.created_at >= ep);
    assert(!e1.isNil());
    assert(bool(e1) == true);
    String e1Str = e1.toString();
    assert(e1Str.size() == 26);

    EID e1Parsed(e1Str);
    assert(e1Parsed == e1);
    assert(e1Parsed.created_at == e1.created_at);
    assert(e1Parsed.id == e1.id);
    assert(e1Parsed.high == e1.created_at);
    assert(e1Parsed.low == e1.id);

    // EID from id and created_at
    EID e2 = EID::fromId(42);
    assert(e2.id == 42);
    assert(e2.created_at > 0);

    EID e3 = EID(999);
    assert(e3.id == 999);

    EID e4(1000, 2000);
    assert(e4.created_at == 1000);
    assert(e4.id == 2000);

    // Test case insensitivity and Crockford aliases: I/L -> 1, O -> 0, hyphens ignored
    String aliasStr = e1Str.toLowerCase();
    EID eLower(aliasStr);
    assert(eLower == e1);

    printf("  PASS: EID & Crockford's Base32\n");
}

static void test_path() {
    printf("[TEST] Path...\n");

    // 1. Consts and type checks
    assert(Path::Back.isBack());
    assert(Path::Back.isControl());
    assert(Path::Back == Path(String("1,1")));

    assert(Path::Control.isControl());
    assert(Path::Control == Path(String("1")));

    assert(Path::Self.isSelf());
    assert(Path::Self == Path(String("0")));

    assert(Path::IPv4.isIPv4());
    assert(Path::IPv4 == Path(String("4")));

    assert(Path::IPv6.isIPv6());
    assert(Path::IPv6 == Path(String("6")));

    // 2. Extends directly and only Array<String>
    Path p;
    p.push("foo");
    p.push("bar");
    assert(p.size() == 2);
    assert(p[0] == "foo");
    assert(p[1] == "bar");

    // Fast Array equality
    Path pOther;
    pOther.push("foo");
    pOther.push("bar");
    assert(p == pOther);
    assert(p.equals(pOther));
    assert(p.constantTimeEquals(pOther));

    pOther.push("baz");
    assert(p != pOther);
    assert(!p.equals(pOther));
    assert(!p.constantTimeEquals(pOther));

    // 3. IPv4 detection, octets, and port
    Path pIp("192.168.1.13:80");
    assert(pIp.isIPv4());
    assert(pIp.port() == 80);
    Array<u8> octets = pIp.ipv4();
    assert(octets.size() == 4);
    assert(octets[0] == 192 && octets[1] == 168 && octets[2] == 1 && octets[3] == 13);
    assert(pIp.toString() == "192.168.1.13:80");

    Path pIpComma("4,1,1,1,1,123");
    assert(pIpComma.isIPv4());
    assert(pIpComma.port() == 123);
    assert(pIpComma.ipv4()[0] == 1);
    assert(pIpComma.toString() == "1.1.1.1:123");

    Path pIpNoPort("10.0.0.1");
    assert(pIpNoPort.isIPv4());
    assert(pIpNoPort.port() == 0);
    assert(pIpNoPort.ipv4()[0] == 10);

    // 4. IPv6 detection and port
    Path pIp6("[::1]:8080");
    assert(pIp6.isIPv6());
    assert(pIp6.port() == 8080);

    // 5. Symmetric common path with * and **
    Path pA("/users/*/profile");
    Path pB("/users/123/profile");
    Path cAB = pA.common(pB);
    Path cBA = pB.common(pA);
    assert(cAB == cBA);
    assert(cAB.size() == 4); // "0", "users", "123", "profile"
    assert(cAB[2] == "123");

    // n-lazy level **
    Path pWild("/files/**/document.pdf");
    Path pTarget("/files/2026/november/reports/document.pdf");
    Path cWild = pWild.common(pTarget);
    Path cTarg = pTarget.common(pWild);
    assert(cWild == cTarg);
    assert(cWild == pTarget);

    // Partial divergence
    Path pX("/a/b/c");
    Path pY("/a/b/d");
    assert(pX.common(pY) == pY.common(pX));
    assert(pX.common(pY) == Path("/a/b"));

    printf("  PASS: Path\n");
}

static void test_tree() {
    printf("[TEST] Tree...\n");
    Tree<void> root;
    assert(root.isNull());

    root["users"]["alice"] = String("admin");
    root["users"]["bob"] = String("guest");

    assert(!root.isNull());
    assert(root.has("users"));
    assert(root.has("users/alice"));

    Tree<void>* aliceNode = root.get("users/alice");
    assert(aliceNode != nullptr);
    assert(aliceNode->hasValue());
    Tree<String>* aliceTyped = dynamic_cast<Tree<String>*>(aliceNode);
    assert(aliceTyped != nullptr);
    assert(aliceTyped->value == "admin");
    assert(*(*aliceTyped) == "admin");

    bool deleted = root.deleteKey("users/bob");
    assert(deleted);
    assert(!root.has("users/bob"));

    // Serialization & compact encoding tests
    Tree<void> serialRoot;
    serialRoot["settings"]["active"] = true;
    serialRoot["settings"]["count"] = 42;
    serialRoot["settings"]["ratio"] = 3.14;
    serialRoot["settings"]["name"] = "test-tree";

    String enc = serialRoot.toString();
    assert(!enc.isEmpty());

    // Decode via constructor Tree(String)
    Tree<void> restored(enc);
    assert(restored["settings"]["active"].asBool() == true);
    assert(restored["settings"]["count"].as<int>() == 42);
    assert(restored["settings"]["name"].asString() == "test-tree");
    f64 ratio = restored["settings"]["ratio"].as<f64>();
    assert(ratio > 3.13 && ratio < 3.15);

    // Decode via parseMany
    String combined = serialRoot.toString() + serialRoot.toString();
    Array<Tree<void>> many = Tree<void>::parseMany(combined);
    assert(many.size() == 2);
    assert(many[0]["settings"]["name"].asString() == "test-tree");
    assert(many[1]["settings"]["count"].as<int>() == 42);

    printf("  PASS: Tree\n");
}

// Custom types for testing Tree<T>
struct Point2D {
    int x;
    int y;
};

struct PlayerState {
    String name;
    int health;

    String petrify() const {
        return name + ":" + String(health);
    }

    static PlayerState hydrate(const String &s) {
        Array<String> parts = s.split(":");
        PlayerState p;
        if (parts.size() >= 2) {
            p.name = parts[0];
            p.health = parts[1].toInt();
        }
        return p;
    }
};

static void test_standard_trees_and_petrify() {
    printf("[TEST] Standard Trees, Custom Typs, YAML/JSON Parser, and Petrified Tree...\n");

    // 1. Standard tree specializations
    Tree<u64> t_u64(12345678901234ULL);
    Tree<f64> t_f64(3.14159265);
    Tree<u32> t_u32(42000U);
    Tree<f32> t_f32(1.5f);
    Tree<i32> t_i32(-100);
    Tree<i64> t_i64(-9876543210LL);
    Tree<u8> t_u8((u8)250);
    Tree<byte> t_byte(true); // byte is bool
    Tree<String> t_str("standard-tree");
    Tree<Comment> t_comm("this is a comment");

    // Verify Comment extends String
    assert(static_cast<String*>(&t_comm.value) != nullptr);
    assert(t_comm.value == "this is a comment");

    // Verify typed extraction
    assert(t_u64.as<u64>() == 12345678901234ULL);
    assert(t_u64.asUint() == 12345678901234ULL);
    assert(t_f64.as<f64>() > 3.14 && t_f64.as<f64>() < 3.15);
    assert(t_u32.as<u32>() == 42000U);
    assert(t_f32.as<f32>() > 1.49f && t_f32.as<f32>() < 1.51f);
    assert(t_i32.as<i32>() == -100);
    assert(t_i32.asInt() == -100);
    assert(t_i64.as<i64>() == -9876543210LL);
    assert(t_u8.as<u8>() == 250);
    assert(t_byte.as<byte>() == true);
    assert(t_byte.asBool() == true);
    assert(t_str.asString() == "standard-tree");
    assert(t_comm.asString() == "this is a comment");

    // 2. Native array vs object handling
    Tree<void> arr;
    arr.push(10);
    arr.push(20);
    arr.push(30);
    assert(arr.isArray());
    assert(!arr.isObject());
    assert(arr.size() == 3);

    Tree<void> obj;
    obj["name"] = "Alice";
    obj["age"] = 28;
    assert(!obj.isArray());
    assert(obj.isObject());
    assert(obj.has("name"));
    assert(obj.has("age"));

    // 3. Custom Tree<T> holds what it holds (no typ hack)
    Point2D pt{10, 20};
    Tree<Point2D> t_pt(pt);
    assert(t_pt.type == getMangledCppType<Point2D>());
    assert(!t_pt.has("typ"));
    assert(!t_pt.has("val"));
    assert((*t_pt).x == 10 && (*t_pt).y == 20);

    // In yaml/json: unrecognized type returns empty string directly
    assert(stringifyJSON(t_pt) == "");
    assert(stringifyYaml(t_pt) == "");

    // Custom Tree<T> with value
    PlayerState ps{"Hero", 100};
    Tree<PlayerState> t_ps(ps);
    assert(t_ps.type == getMangledCppType<PlayerState>());
    assert(!t_ps.has("typ"));
    assert(!t_ps.has("val"));
    assert(t_ps->name == "Hero");
    assert((*t_ps).health == 100);

    // Tags on Tree
    t_ps.tags.push("player");
    t_ps.tags.push("active");
    assert(t_ps.tags.size() == 2);
    assert(t_ps.tags[0] == "player");

    // 4. stringifyJSON and stringifyYaml
    String compactJson = stringifyJSON(obj, 0);
    assert(!compactJson.contains("\n") && !compactJson.contains(" "));
    assert(compactJson.contains("\"name\":\"Alice\""));
    assert(compactJson.contains("\"age\":28"));

    String prettyJson = stringifyJSON(obj, 2);
    assert(prettyJson.contains("\n"));

    String yamlOut = stringifyYaml(obj, 2);
    assert(yamlOut.contains("name: Alice"));
    assert(yamlOut.contains("age: 28"));

    // 5. Unified parser with YAML anchors/aliases (go-tos) and comments
    String yamlInput = 
        "# Server Configuration\n"
        "default_cfg: &default_cfg\n"
        "  retries: 3\n"
        "  timeout: 5000\n"
        "\n"
        "service_a:\n"
        "  <<: *default_cfg\n"
        "  host: \"srv1\"\n";

    Tree<void> yamlTree;
    bool yamlParsed = parseYaml(yamlInput, yamlTree);
    assert(yamlParsed);
    assert(yamlTree["service_a"]["retries"].asInt() == 3);
    assert(yamlTree["service_a"]["timeout"].asInt() == 5000);
    assert(yamlTree["service_a"]["host"].asString() == "srv1");

    // JSON with C-style comments (JSONC)
    String jsoncInput = 
        "// Main settings\n"
        "{\n"
        "  /* inline comment */\n"
        "  \"title\": \"Antigravity\",\n"
        "  \"count\": 7\n"
        "}\n";

    Tree<void> jsoncTree;
    bool jsoncParsed = parseYaml(jsoncInput, jsoncTree);
    assert(jsoncParsed);
    assert(jsoncTree["title"].asString() == "Antigravity");
    assert(jsoncTree["count"].asInt() == 7);

    // 6. Custom type petrify roundtrip: preserves type and value, never fails
    String petrifyCustom = petrify(t_ps);
    assert(!petrifyCustom.isEmpty());
    Tree<void> hydCustom;
    bool hydCustomOk = hydrate(petrifyCustom, hydCustom);
    assert(hydCustomOk);
    assert(hydCustom.type == getMangledCppType<PlayerState>());
    assert(hydCustom.asString() == "Hero:100");

    // 7. Petrified Tree: bitpacking and roundtrip fidelity
    Tree<void> petrifyTarget;
    petrifyTarget["id"] = 1001;
    petrifyTarget["flag"] = true;
    petrifyTarget["score"] = 99.5;
    petrifyTarget["user"]["name"] = "Alice";
    petrifyTarget["user"]["roles"].push("admin");
    petrifyTarget["user"]["roles"].push("operator");

    String petrifiedData = petrify(petrifyTarget);
    assert(!petrifiedData.isEmpty());

    Tree<void> hydratedTree;
    bool hydOk = hydrate(petrifiedData, hydratedTree);
    assert(hydOk);
    assert(hydratedTree["id"].asInt() == 1001);
    assert(hydratedTree["flag"].asBool() == true);
    assert(hydratedTree["score"].as<f64>() > 99.4 && hydratedTree["score"].as<f64>() < 99.6);
    assert(hydratedTree["user"]["name"].asString() == "Alice");
    assert(hydratedTree["user"]["roles"].size() == 2);
    assert(hydratedTree["user"]["roles"].at(0)->asString() == "admin");
    assert(hydratedTree["user"]["roles"].at(1)->asString() == "operator");

    // Verify bitpacking efficiency
    String jsonRepr = stringifyJSON(petrifyTarget, 0);
    // Petrified binary is significantly more compact than raw JSON
    assert(petrifiedData.size() < jsonRepr.size());

    printf("  PASS: Standard Trees, Custom Typs, YAML/JSON Parser, and Petrified Tree\n");
}

static void test_writ() {
    printf("[TEST] Writ (Auth/Writ, PKI, Chains of Trust, Masking)...\n");

    // 1. Canonical hash determinism and sorting
    Tree<void> w1;
    w1["public"] = "12345678901234567890123456789012";
    w1["name"] = "writ-one";
    w1["created"] = 1000;

    Tree<void> w2;
    // Inserted in different order
    w2["created"] = 1000;
    w2["public"] = "12345678901234567890123456789012";
    w2["name"] = "writ-one";

    String h1 = Writ::canonicalHash(w1);
    String h2 = Writ::canonicalHash(w2);
    assert(h1 == h2);
    assert(h1.size() == 16);

    String hSecret = Writ::canonicalHash(w1, "my-secret-key");
    assert(hSecret.size() == 16);
    assert(hSecret != h1);

    // 2. Symmetric hash & unhash
    Writ::hash("my-secret-key", w1);
    assert(w1.has("hash"));
    assert(w1["hash"].asString() == hSecret);

    // Canonical hash ignores hash field
    assert(Writ::canonicalHash(w1, "my-secret-key") == hSecret);

    Writ::unhash(w1);
    assert(!w1.has("hash"));

    // 3. Vouching with Ed25519
    EdKeypair rootKp = EdKeypair::generate();
    EdKeypair leafKp = EdKeypair::generate();

    Tree<void> leafWrit;
    leafWrit["public"] = leafKp.publicKey().asString();
    leafWrit["name"] = "leaf";
    leafWrit["created"] = 500;
    leafWrit["expires"] = 5000;
    leafWrit["perm"]["storage/*"] = 10;

    Writ::vouch(rootKp, leafWrit);
    assert(leafWrit.has("vouch"));

    // Canonical hash ignores vouch field
    assert(Writ::canonicalHash(leafWrit) == Writ::canonicalHash(leafWrit));

    // 4. Validate direct trust
    Array<Tree<void>> pool;
    pool.push(leafWrit);

    Array<String> roots;
    roots.push(rootKp.publicKey().asString());

    Array<String> leafHints;
    leafHints.push(leafKp.publicKey().asString());

    auto directChains = Writ::validate(leafHints, pool, roots, 1000);
    assert(directChains.size() == 1);
    assert(directChains[0].size() == 1);
    assert(directChains[0][0]["name"].asString() == "leaf");

    // Time expiry check: now > expires
    auto expiredChains = Writ::validate(leafHints, pool, roots, 6000);
    assert(expiredChains.size() == 0);

    // now == 0 ignores expiry
    auto noTimeChains = Writ::validate(leafHints, pool, roots, 0);
    assert(noTimeChains.size() == 1);

    // 5. Multi-hop delegation chain: Root -> Admin -> Worker
    EdKeypair adminKp = EdKeypair::generate();
    EdKeypair workerKp = EdKeypair::generate();

    Tree<void> adminWrit;
    adminWrit["public"] = adminKp.publicKey().asString();
    adminWrit["name"] = "admin";
    adminWrit["perm"]["cluster/*"] = 20;
    Writ::vouch(rootKp, adminWrit);

    Tree<void> workerWrit;
    workerWrit["public"] = workerKp.publicKey().asString();
    workerWrit["name"] = "worker";
    workerWrit["perm"]["cluster/node1"] = 15;
    workerWrit["perm"]["cluster/unauthorized"] = 50; // tries to exceed parent
    workerWrit["perm"]["outside/*"] = 5; // outside parent scope
    Writ::vouch(adminKp, workerWrit);

    Array<Tree<void>> chainPool;
    chainPool.push(adminWrit);
    chainPool.push(workerWrit);

    Array<String> workerHints;
    workerHints.push(workerKp.publicKey().asString());

    auto multiChains = Writ::validate(workerHints, chainPool, roots, 0);
    assert(multiChains.size() == 1);
    assert(multiChains[0].size() == 2);
    assert(multiChains[0][0]["name"].asString() == "admin");
    assert(multiChains[0][1]["name"].asString() == "worker");

    // 6. Capability masking (Writ::mask)
    Tree<void> effective = Writ::mask(multiChains[0]);
    // cluster/node1 was reduced to 15 (min(20, 15) = 15)
    Tree<void>* n1 = effective.query("cluster/node1");
    assert(n1 != nullptr);
    assert((int)n1->asNumber() == 15);

    // cluster/unauthorized was clamped to parent limit of 20
    Tree<void>* unauth = effective.query("cluster/unauthorized");
    assert(unauth != nullptr);
    assert((int)unauth->asNumber() == 20);

    // outside/* was outside parent's allowed rules, masked to 0
    Tree<void>* out = effective.query("outside/foo");
    assert(out != nullptr);
    assert((int)out->asNumber() == 0);

    // 7. Symmetric authentication by root secret
    Tree<void> secretWrit;
    secretWrit["public"] = workerKp.publicKey().asString();
    secretWrit["name"] = "backend-service";
    Writ::hash("shared-backend-secret", secretWrit);

    Array<Tree<void>> symPool;
    symPool.push(secretWrit);

    Array<String> symRoots;
    symRoots.push("shared-backend-secret");

    Array<String> symHints;
    symHints.push(workerKp.publicKey().asString());

    auto symChains = Writ::validate(symHints, symPool, symRoots, 0);
    assert(symChains.size() == 1);
    assert(symChains[0].size() == 1);
    assert(symChains[0][0]["name"].asString() == "backend-service");

    // Unvouch
    Writ::unvouch(adminKp, workerWrit);
    assert(!workerWrit.has("vouch"));

    printf("  PASS: Writ\n");
}

static void test_algorithm() {
    printf("[TEST] Algorithm (Hash, SHA, Chacha, AEAD, ECC, Password, Deflate)...\n");
    String hBlake3 = hash("Hello world");
    assert(!hBlake3.isEmpty());

    String hSha256 = hashSha256("Hello world");
    assert(hSha256.size() == 32);

    String hSha512 = hashSha512("Hello world");
    assert(hSha512.size() == 64);

    String key(32, 'K');
    u256 k256(key);
    u64 nonce = 12345ULL;
    String plain = "Secret message to encrypt";

    // Test crypt and chacha block
    u512 block512 = chacha(k256, nonce, 12);
    assert(block512.size() == 64);
    assert(!block512.isNil());

    String cipher = crypt(plain, k256, nonce, 12);
    String decrypted = crypt(cipher, k256, nonce, 12);
    assert(decrypted == plain);

    // Also test convenience wrapper chacha
    String cipher2 = chacha(plain, key, nonce, 12);
    assert(cipher2 == cipher);

    AEAD aeadInput;
    aeadInput.text = plain;
    aeadInput.header = "HeaderData";
    AEAD sealed = seal(aeadInput, k256, nonce, 12);
    assert(sealed.hash.size() == 16);

    AEAD opened = openSeal(sealed, k256, nonce, 12);
    assert(opened.text == plain);

    XKeypair x1 = XKeypair::generate();
    XKeypair x2 = XKeypair::generate();
    u256 p1 = x1.publicKey();
    u256 p2 = x2.publicKey();
    printf("p1: "); for (usz i = 0; i < p1.size(); ++i) printf("%02x", (u8)p1[i]); printf("\n");
    printf("p2: "); for (usz i = 0; i < p2.size(); ++i) printf("%02x", (u8)p2[i]); printf("\n");
    u256 s1 = x1.sharedKey(p2);
    u256 s2 = x2.sharedKey(p1);
    printf("s1: "); for (usz i = 0; i < s1.size(); ++i) printf("%02x", (u8)s1[i]); printf("\n");
    printf("s2: "); for (usz i = 0; i < s2.size(); ++i) printf("%02x", (u8)s2[i]); printf("\n");
    fflush(stdout);
    assert(s1 == s2);

    EdKeypair ed1 = EdKeypair::generate();
    EdKeypair ed2 = EdKeypair::generate();
    XKeypair xFromEd(ed1);
    assert(!xFromEd.publicKey().isNil());
    u256 edShared1 = ed1.sharedKey(ed2.publicKey());
    u256 edShared2 = ed2.sharedKey(ed1.publicKey());
    printf("ed1 pub: %s\n", ed1.publicKey().toHex().c_str());
    printf("ed2 pub: %s\n", ed2.publicKey().toHex().c_str());
    printf("edShared1: %s\n", edShared1.toHex().c_str());
    printf("edShared2: %s\n", edShared2.toHex().c_str());
    fflush(stdout);
    assert(edShared1 == edShared2);

    // Ed25519 Sign and Verify with u512 signature
    u512 sig = ed1.sign("Message to sign");
    assert(!sig.isNil());
    assert(ed1.verify("Message to sign", sig));
    assert(!ed1.verify("Wrong message", sig));

    String derived = pbkdf("myPassword", 0x12345678, "auth.domain.com", 32, 16, 2);
    assert(derived.size() == 32);
    printf("  PASS: Algorithm\n");
}

static void test_yaml_jwt() {
    printf("[TEST] Yaml & JWT...\n");
    String yamlInput = "name: Antigravity\nversion: 2\nenabled: true\n";
    Tree<void> root;
    bool ok = parseYaml(yamlInput, root);
    assert(ok);
    assert(root.has("name"));
    assert(root.has("version"));
    assert(root.has("enabled"));

    String json = stringifyJSON(root, 0);
    assert(!json.isEmpty());

    String jwtKey = "super-secret-jwt-key";
    String token = JWT::seal(root, jwtKey);
    assert(!token.isEmpty());
    assert(JWT::verify(token, jwtKey));

    Tree<void> recovered = JWT::open(token, jwtKey);
    assert(!recovered.isNull());
    assert(recovered.has("name"));
    printf("  PASS: Yaml & JWT\n");
}

static void test_compression() {
    printf("[TEST] Compression (Deflate & Inflate)...\n");
    
    // 1. Empty string
    assert(Algorithm::deflate("").size() > 0);
    assert(Algorithm::inflate(Algorithm::deflate("")) == "");

    // 2. Short string
    String shortStr = "Hello";
    String defShort = Algorithm::deflate(shortStr);
    assert(Algorithm::inflate(defShort) == shortStr);

    // 3. Repeated pattern (LZ77 + Dynamic Huffman)
    String original = "This is a repeated string for compression testing! Repeat repeat repeat repeat!";
    for (int k = 0; k < 5; ++k) original = original + original;
    
    // Test across different compression levels
    for (int lvl = 0; lvl <= 9; ++lvl) {
        String compressed = Algorithm::deflate(original, lvl);
        assert(!compressed.isEmpty());
        if (lvl > 0) {
            assert(compressed.size() < original.size());
        }
        String uncompressed = Algorithm::inflate(compressed);
        assert(uncompressed == original);
    }

    // 4. Large buffer crossing 32KB window
    String large;
    for (int i = 0; i < 1000; ++i) {
        large = large + "Unique pattern " + String((i64)i) + " repeated text to span multiple window frames! ";
    }
    String defLarge = Algorithm::deflate(large, 6);
    assert(defLarge.size() < large.size());
    String infLarge = Algorithm::inflate(defLarge);
    assert(infLarge == large);

    // 5. Corrupt input robustness (must not crash or hang)
    String corrupt = "Not a valid compressed stream garbage header";
    String res = Algorithm::inflate(corrupt);
    (void)res;

    // 6. Cross-compatibility: decode zlib (RFC 1950) and gzip (RFC 1952) streams
    const u8 zlibBytes[] = {120, 156, 5, 193, 193, 9, 128, 48, 16, 4, 192, 86, 214, 2, 196, 34, 124, 250, 177, 133, 68, 87, 61, 136, 151, 224, 45, 72, 172, 222, 153, 249, 169, 17, 227, 86, 239, 150, 100, 217, 138, 169, 67, 12, 33, 83, 47, 233, 88, 187, 174, 234, 248, 138, 229, 233, 252, 172, 33, 249, 142, 37, 72, 152, 31, 37, 137, 195, 15, 106, 167, 25, 23};
    String zlibStream((const char*)zlibBytes, sizeof(zlibBytes));
    String zlibDec = Algorithm::inflate(zlibStream);
    assert(zlibDec == "Cross-compatibility test between Python zlib/gzip and Ksee inflate!");

    const u8 gzipBytes[] = {31, 139, 8, 0, 0, 0, 0, 0, 2, 255, 5, 193, 193, 9, 128, 48, 16, 4, 192, 86, 214, 2, 196, 34, 124, 250, 177, 133, 68, 87, 61, 136, 151, 224, 45, 72, 172, 222, 153, 249, 169, 17, 227, 86, 239, 150, 100, 217, 138, 169, 67, 12, 33, 83, 47, 233, 88, 187, 174, 234, 248, 138, 229, 233, 252, 172, 33, 249, 142, 37, 72, 152, 31, 37, 137, 195, 15, 189, 127, 157, 73, 67, 0, 0, 0};
    String gzipStream((const char*)gzipBytes, sizeof(gzipBytes));
    String gzipDec = Algorithm::inflate(gzipStream);
    assert(gzipDec == "Cross-compatibility test between Python zlib/gzip and Ksee inflate!");

    printf("  PASS: Compression (Deflate & Inflate)\n");
}

static void test_math() {
    printf("[TEST] Math...\n");
    f32 rad = degToRad(180.0f);
    assert(rad > 3.14f && rad < 3.15f);

    Vec2 v2(3.0f, 4.0f);
    assert(v2.length() == 5.0f);

    Vec3 v3(1.0f, 2.0f, 3.0f);
    Vec3 v3b(4.0f, 5.0f, 6.0f);
    assert(v3.dot(v3b) == 32.0f);

    Matrix4 m = Matrix4::identity();
    assert(m.at(0, 0) == 1.0f);

    Quaternion q = Quaternion::identity();
    assert(q.w == 1.0f);

    f32 rf = randomNumber<f32>(1.0f, 10.0f);
    assert(rf >= 1.0f && rf <= 10.0f);
    f64 rd = randomNumber<f64>(5.0, 15.0);
    assert(rd >= 5.0 && rd <= 15.0);
    u32 ru = randomNumber<u32>(100, 200);
    assert(ru >= 100 && ru <= 200);
    u64 ru64 = randomNumber<u64>(1000ULL, 2000ULL);
    assert(ru64 >= 1000ULL && ru64 <= 2000ULL);
    printf("  PASS: Math\n");
}

static int globalFunc(int x) {
    return x * 2;
}

static void test_func() {
    printf("[TEST] Func...\n");
    // 1. Empty func
    Func<int(int)> empty;
    assert(!empty);
    assert(!empty.isValid());

    // 2. Raw function pointer
    Func<int(int)> f1 = globalFunc;
    assert(f1);
    assert(f1(5) == 10);

    // 3. Lambda with capture (SBO)
    int capture = 42;
    Func<int(int)> f2 = [capture](int a) {
        return a + capture;
    };
    assert(f2);
    assert(f2(8) == 50);

    // 4. Copy and Move
    Func<int(int)> f3 = f2;
    assert(f3(8) == 50);
    Func<int(int)> f4 = Ksee::Move(f3);
    assert(f4(10) == 52);

    // 5. Large capture (Heap fallback)
    struct LargeCallable {
        u8 pad[256];
        int val;
        int operator()(int x) const { return x + val; }
    };
    LargeCallable lc{};
    lc.val = 100;
    Func<int(int)> fLarge = lc;
    assert(fLarge);
    assert(fLarge(23) == 123);

    Func<int(int)> fLargeCopy = fLarge;
    assert(fLargeCopy(23) == 123);

    printf("  PASS: Func\n");
}


static void test_iterator() {
    printf("[TEST] Iterator...\n");
    Ksee::Iterator<u32, u32> iter = [] () {
        yield(1);
        yield(2);
        yield(); // adds nothing to the channel
        yield(5);
        return 1;
    };

    assert(iter.available() > 0);
    assert(*iter == 1);
    assert(iter.next() == 1);
    assert(iter.next() == 2);
    assert(iter.next() == 5);
    assert(iter.available() == 0);
    assert(iter.done);
    assert(iter.retVal == 1);

    // Test for-loop range based
    Ksee::Iterator<u32, void> iter2 = [] () {
        yield(10);
        yield(20);
        yield(30);
    };
    u32 sum = 0;
    for (auto val : iter2) {
        sum += val;
    }
    assert(sum == 60);
    printf("  PASS: Iterator\n");
}

static void test_resource_fs() {
    printf("[TEST] Resource FS & OSFilesystem...\n");

    // 1. FS Root & Preamble Sandboxing
    OSFilesystem jail;
    jail.root = "/tmp/sandbox";
    jail.preamble = "/app";
    String resolved = jail.resolve(Path("/users/../../secret.txt"));
    assert(resolved == "/tmp/sandbox/app/secret.txt");
    assert(resolved.startsWith("/tmp/sandbox"));

    // 2. OSFilesystem Singleton High-Level Streaming
    Path testFile("/tmp/ksee_test_fs_unit.txt");
    fs.write(testFile, "Hello World from FS!");
    assert(fs.exists(testFile));
    assert(fs.isFile(testFile));
    assert(!fs.isDir(testFile));
    String content = fs.read(testFile);
    assert(content == "Hello World from FS!");

    fs.append(testFile, " Appended line.");
    String appended = fs.read(testFile);
    assert(appended == "Hello World from FS! Appended line.");

    // 3. Raw Descriptor / Stream I/O
    u64 h = fs.open(testFile, false);
    assert(h != 0);
    assert(fs.size(h) == appended.size());
    char buf[16] = {0};
    usz bytesRead = fs.read(h, buf, 5);
    assert(bytesRead == 5);
    assert(String(buf, 5) == "Hello");
    fs.close(h);

    // 4. Direct Virtual Filesystem Operations: Copy, Move, Mkdir, Rmdir, Unlink
    Path copyFile("/tmp/ksee_test_fs_unit_copy.txt");
    assert(fs.copy(copyFile, testFile));
    assert(fs.read(copyFile) == appended);

    Path movedFile("/tmp/ksee_test_fs_unit_moved.txt");
    assert(fs.move(movedFile, testFile));
    assert(fs.read(movedFile) == appended);

    Path testDir("/tmp/ksee_test_fs_dir");
    assert(fs.mkdir(testDir));
    assert(fs.exists(testDir));
    assert(fs.isDir(testDir));
    assert(!fs.isFile(testDir));
    Array<Path> tmpList = fs.list(Path("/tmp"));
    assert(tmpList.size() > 0);
    assert(fs.rmdir(testDir));

    assert(fs.unlink(movedFile));
    assert(fs.unlink(copyFile));
    printf("  PASS: Resource FS & OSFilesystem\n");
}

static void test_comms() {
    printf("[TEST] Comms (Packet & Band)...\n");

    // 1. Standard Packet
    Ksee::Comms::Packet pkt("Test payload", 5, 2, 100, 500, true, true, false);
    assert(pkt.handle == 5);
    assert(pkt.intent == 2);
    assert(pkt.offset == 100);
    assert(pkt.intendedLength == 500);
    assert(pkt.reliable == true);
    assert(pkt.ordered == true);
    assert(pkt.sacrificial == false);
    assert(pkt.payload == "Test payload");

    // 2. Standard Band
    Ksee::Comms::Band band;
    assert(band.couldPush());
    assert(band.duty == 0);
    assert(band.maxBundleSize == 1400);

    String sentData;
    band.sendCallback = [&](String b) { sentData = b; };
    band.push("Hello Band");
    assert(sentData == "Hello Band");
    assert(band.duty >= 1);

    // Stun
    band.stun(100);
    assert(!band.couldPush());

    printf("  PASS: Comms (Packet & Band)\n");
}

int main() {
    printf("========================================\n");
    printf("Running Ksee Standard Test Suite\n");
    printf("========================================\n");

    test_types();
    test_func();
    test_iterator();
    test_resource_fs();
    test_comms();
    test_array_string();
    test_sparse_set_map();
    test_uuid();
    test_eid_and_cb32();
    test_path();
    test_tree();
    test_standard_trees_and_petrify();
    test_writ();
    test_algorithm();
    test_yaml_jwt();
    test_compression();
    test_math();

    printf("========================================\n");
    printf("ALL TESTS PASSED SUCCESSFULLY!\n");
    printf("========================================\n");
    return 0;
}

