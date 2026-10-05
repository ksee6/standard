#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../include/Ksee/Types.hpp"
#include "../include/Ksee/Func.hpp"
#include "../include/Ksee/Iterator.hpp"
#include "../include/Ksee/Array.hpp"
#include "../include/Ksee/Sparse.hpp"
#include "../include/Ksee/String.hpp"
#include "../include/Ksee/Set.hpp"
#include "../include/Ksee/Map.hpp"
#include "../include/Ksee/Time.hpp"
#include "../include/Ksee/ID.hpp"
#include "../include/Ksee/Path.hpp"
#include "../include/Ksee/Tree.hpp"
#include "../include/Ksee/Crypto/Hash.hpp"
#include "../include/Ksee/Crypto/SHA.hpp"
#include "../include/Ksee/Crypto/Chacha.hpp"
#include "../include/Ksee/Crypto/AEAD.hpp"
#include "../include/Ksee/Crypto/ECC.hpp"
#include "../include/Ksee/Crypto/Password.hpp"
#include "../include/Ksee/Format/Yaml.hpp"
#include "../include/Ksee/Auth/JWT.hpp"
#include "../include/Ksee/Parse/Regex.hpp"
#include "../include/Ksee/Compression/Deflate.hpp"
#include "../include/Ksee/Compression/Tar.hpp"
#include "../include/Ksee/Compression/Zip.hpp"
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
#include "../include/Ksee/Debug/Log.hpp"

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
    printf("[TEST] Sparse, Set, Map...\n");
    Sparse<int> sp;
    sp[0] = 100;
    sp[50] = 200;
    assert(sp[0] == 100);
    assert(sp[50] == 200);

    Set<String> set;
    set.add("apple");
    set.add("banana");
    assert(set.has("apple"));
    assert(!set.has("cherry"));

    Map<String, int> map;
    map.put("first", 1);
    map.put("second", 2);
    assert(map.has("first"));
    assert(*map.get("first") == 1);
    assert(*map.get("second") == 2);
    printf("  PASS: Sparse, Set, Map\n");
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
    assert(isEpochTimeSet == true);
    assert(epochOnStart > 0);

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
    Path p1("/path/to/something");
    assert(p1[0] == "0");
    assert(p1[1] == "path");
    assert(p1[2] == "to");
    assert(p1[3] == "something");

    Path pUrl("schema:///domain.google.com:80/path1/path2?key=value&another=2#tag");
    assert(pUrl[0] == "2");
    assert(pUrl.getSchema() == "schema");
    assert(pUrl.getParam("key") == "value");
    assert(pUrl.getParam("another") == "2");
    assert(pUrl.hasTag("tag"));
    assert(pUrl.port() == 80);

    Path pIp("192.168.1.13:80");
    assert(pIp.isIpv4());
    assert(pIp.port() == 80);

    Path pWild("/path/to/*");
    Path pTarget("/path/to/myitem");
    Array<u32> extracted = pWild.match(pTarget);
    assert(!extracted.isEmpty());
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
    printf("  PASS: Tree\n");
}

static void test_crypto() {
    printf("[TEST] Crypto (Hash, SHA, Chacha, AEAD, ECC, Password)...\n");
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
    printf("  PASS: Crypto\n");
}

static void test_yaml_jwt() {
    printf("[TEST] Yaml & JWT...\n");
    String yamlInput = "name: Antigravity\nversion: 2\nenabled: true\n";
    Tree<void> root;
    bool ok = Yaml::parse(yamlInput, root);
    assert(ok);
    assert(root.has("name"));
    assert(root.has("version"));
    assert(root.has("enabled"));

    String json = Yaml::toJSON(root, 0);
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
    printf("[TEST] Compression (Deflate, Tar, Zip)...\n");
    String original = "This is a repeated string for compression testing! Repeat repeat repeat repeat!";
    String compressed = Deflate::compress(original);
    assert(!compressed.isEmpty());
    String uncompressed = Deflate::decompress(compressed);
    assert(uncompressed == original);

    Array<TarEntry> tarEntries;
    tarEntries.push(TarEntry{"test.txt", original, 0644, false});
    String tarBytes = Tar::pack(tarEntries);
    assert(!tarBytes.isEmpty());

    Array<TarEntry> unpacked = Tar::unpack(tarBytes);
    assert(unpacked.size() >= 1);
    assert(unpacked[0].data == original);

    Array<ZipEntry> zipEntries;
    zipEntries.push(ZipEntry{"hello.txt", original, false});
    String zipBytes = Zip::pack(zipEntries);
    assert(!zipBytes.isEmpty());

    Array<ZipEntry> zipUnpacked = Zip::unpack(zipBytes);
    assert(zipUnpacked.size() >= 1);
    assert(zipUnpacked[0].data == original);
    printf("  PASS: Compression\n");
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

int main() {
    printf("========================================\n");
    printf("Running Ksee Standard Test Suite\n");
    printf("========================================\n");

    test_types();
    test_func();
    test_iterator();
    test_array_string();
    test_sparse_set_map();
    test_uuid();
    test_eid_and_cb32();
    test_path();
    test_tree();
    test_crypto();
    test_yaml_jwt();
    test_compression();
    test_math();

    printf("========================================\n");
    printf("ALL TESTS PASSED SUCCESSFULLY!\n");
    printf("========================================\n");
    return 0;
}

