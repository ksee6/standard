/**
 * @file Writ.cpp
 * @brief Trust token and capability delegation tree (Writ) implementation for Ksee.
 */

#include "../../../include/Ksee/Auth/Writ.hpp"

#include <functional>
#include <cstdio>

namespace Ksee {

static String canonicalBytes(const Tree<void> &writ) {
    Tree<void> stripped = writ;
    for (long long i = (long long)stripped.children.size() - 1; i >= 0; --i) {
        String p = stripped.children[(usz)i]->path.toString();
        if (p == "vouch" || p == "hash") {
            delete stripped.children[(usz)i];
            stripped.children.remove((usz)i);
        }
    }
    return stripped.toBinary();
}

static String vouchMessage(const Tree<void> &writ) {
    Tree<void> stripped = writ;
    for (long long i = (long long)stripped.children.size() - 1; i >= 0; --i) {
        String p = stripped.children[(usz)i]->path.toString();
        if (p == "vouch") {
            delete stripped.children[(usz)i];
            stripped.children.remove((usz)i);
        }
    }
    return stripped.toBinary();
}

String Writ::canonicalHash(const Tree<void> &writ, const String &secret) {
    String bytes = canonicalBytes(writ);
    String data = bytes + secret;
    return Ksee::hash(data, 16);
}

void Writ::vouch(const EdKeypair &us, Tree<void> &writ) {
    String msg = vouchMessage(writ);
    u512 sig = us.sign(msg);
    String pub32 = us.publicKey().asString();
    String pub8 = pub32.substring(0, 8);
    String vouchData = pub8 + sig.asString();

    writ.children.push(new Tree<String>(Path("vouch"), vouchData));
}

void Writ::unvouch(const EdKeypair &us, Tree<void> &writ) {
    String pub8 = us.publicKey().asString().substring(0, 8);
    for (long long i = (long long)writ.children.size() - 1; i >= 0; --i) {
        if (writ.children[(usz)i]->path.toString() == "vouch") {
            if (writ.children[(usz)i]->asString().startsWith(pub8)) {
                delete writ.children[(usz)i];
                writ.children.remove((usz)i);
            }
        }
    }
}

void Writ::unvouch(Tree<void> &writ) {
    for (long long i = (long long)writ.children.size() - 1; i >= 0; --i) {
        if (writ.children[(usz)i]->path.toString() == "vouch") {
            delete writ.children[(usz)i];
            writ.children.remove((usz)i);
        }
    }
}

void Writ::hash(const EdKeypair &us, Tree<void> &writ) {
    writ["hash"] = canonicalHash(writ, us.secretKey.asString());
}

void Writ::hash(const String &secret, Tree<void> &writ) {
    writ["hash"] = canonicalHash(writ, secret);
}

void Writ::unhash(const EdKeypair &us, Tree<void> &writ) {
    (void)us;
    writ.deleteKey("hash");
}

void Writ::unhash(const String &secret, Tree<void> &writ) {
    (void)secret;
    writ.deleteKey("hash");
}

void Writ::unhash(Tree<void> &writ) {
    writ.deleteKey("hash");
}

struct VouchInfo {
    String pub8;
    String sig64;
};

static Array<VouchInfo> extractVouches(const Tree<void> &w) {
    Array<VouchInfo> list;
    for (usz i = 0; i < w.children.size(); ++i) {
        if (w.children[i]->path.toString() == "vouch") {
            String val = w.children[i]->asString();
            if (val.size() >= 72) {
                list.push({val.substring(0, 8), val.substring(8, 72)});
            }
            for (usz c = 0; c < w.children[i]->children.size(); ++c) {
                String cval = w.children[i]->children[c]->asString();
                if (cval.size() >= 72) {
                    list.push({cval.substring(0, 8), cval.substring(8, 72)});
                }
            }
        }
    }
    return list;
}

Sparse<Sparse<Tree<void>>> Writ::validate(
    const Array<String> &leafPublicHints,
    const Array<Tree<void>> &allWrits,
    const Array<String> &roots,
    u64 now
) {
    Sparse<Sparse<Tree<void>>> allChains;
    usz n = allWrits.size();
    if (n == 0) return allChains;

    // 1. Time validity
    Array<bool> validTime;
    validTime.allocate(n);
    for (usz i = 0; i < n; ++i) {
        validTime[i] = true;
        if (now > 0) {
            Tree<void>* startsNode = allWrits[i].get("starts");
            if (startsNode && now < (u64)startsNode->asNumber()) {
                validTime[i] = false;
                continue;
            }
            Tree<void>* expiresNode = allWrits[i].get("expires");
            if (expiresNode && now > (u64)expiresNode->asNumber()) {
                validTime[i] = false;
                continue;
            }
        }
    }

    // 2. Identify writs directly trusted by roots
    Array<bool> isRootTrusted;
    isRootTrusted.allocate(n);
    for (usz i = 0; i < n; ++i) {
        isRootTrusted[i] = false;
        if (!validTime[i]) continue;

        const Tree<void> &w = allWrits[i];

        // Symmetric hash check
        Tree<void>* hashNode = w.get("hash");
        if (hashNode) {
            String actualHash = hashNode->asString();
            for (usz r = 0; r < roots.size(); ++r) {
                if (canonicalHash(w, roots[r]) == actualHash) {
                    isRootTrusted[i] = true;
                    break;
                }
            }
            if (isRootTrusted[i]) continue;
        }

        // Direct public key match with a root
        Tree<void>* pubNode = w.get("public");
        String writPub = pubNode ? pubNode->asString() : String();
        if (!writPub.isEmpty()) {
            for (usz r = 0; r < roots.size(); ++r) {
                if (roots[r] == writPub) {
                    isRootTrusted[i] = true;
                    break;
                }
                if (roots[r].size() == 32) {
                    EdKeypair kp(roots[r]);
                    if (kp.publicKey().asString() == writPub) {
                        isRootTrusted[i] = true;
                        break;
                    }
                }
            }
            if (isRootTrusted[i]) continue;
        }

        // Root vouch check
        String msg = vouchMessage(w);
        Array<VouchInfo> vouches = extractVouches(w);
        for (usz v = 0; v < vouches.size() && !isRootTrusted[i]; ++v) {
            for (usz r = 0; r < roots.size(); ++r) {
                const String &rootKey = roots[r];
                if (rootKey.size() == 32) {
                    if (rootKey.startsWith(vouches[v].pub8) && edVerify(rootKey, msg, vouches[v].sig64)) {
                        isRootTrusted[i] = true;
                        break;
                    }
                    EdKeypair kp(rootKey);
                    String derivedPub = kp.publicKey().asString();
                    if (derivedPub.startsWith(vouches[v].pub8) && edVerify(derivedPub, msg, vouches[v].sig64)) {
                        isRootTrusted[i] = true;
                        break;
                    }
                } else {
                    if (rootKey.startsWith(vouches[v].pub8) && edVerify(rootKey, msg, vouches[v].sig64)) {
                        isRootTrusted[i] = true;
                        break;
                    }
                }
            }
        }
    }

    // 3. Parent-child delegation edges
    Array<Array<usz>> childrenOf;
    childrenOf.allocate(n);
    for (usz i = 0; i < n; ++i) {
        if (!validTime[i]) continue;
        Tree<void>* pubNode = allWrits[i].get("public");
        if (!pubNode) continue;
        String pub = pubNode->asString();
        if (pub.size() != 32) continue;

        for (usz c = 0; c < n; ++c) {
            if (i == c || !validTime[c]) continue;
            String msg_c = vouchMessage(allWrits[c]);
            Array<VouchInfo> vouches_c = extractVouches(allWrits[c]);
            for (usz v = 0; v < vouches_c.size(); ++v) {
                if (pub.startsWith(vouches_c[v].pub8) && edVerify(pub, msg_c, vouches_c[v].sig64)) {
                    childrenOf[i].push(c);
                    break;
                }
            }
        }
    }

    // 4. Candidate leaf selection
    Array<usz> candidateLeaves;
    for (usz i = 0; i < n; ++i) {
        if (!validTime[i]) continue;
        if (leafPublicHints.isEmpty()) {
            candidateLeaves.push(i);
        } else {
            Tree<void>* pubNode = allWrits[i].get("public");
            String pub = pubNode ? pubNode->asString() : String();
            bool matched = false;
            for (usz h = 0; h < leafPublicHints.size(); ++h) {
                const String &hint = leafPublicHints[h];
                if (pub == hint || pub.startsWith(hint)) {
                    matched = true;
                    break;
                }
                if (pub.size() == 32 && hint.size() == 64 && u256((const u8*)pub.data()).toHex() == hint) {
                    matched = true;
                    break;
                }
            }
            if (matched) {
                candidateLeaves.push(i);
            }
        }
    }

    // 5. Discover all valid chains of trust
    std::function<void(usz, usz, Array<usz>&, Array<bool>&)> dfs =
        [&](usz curr, usz target, Array<usz> &path, Array<bool> &visited) {
            if (curr == target) {
                Sparse<Tree<void>> chain;
                for (usz p = 0; p < path.size(); ++p) {
                    chain.push(allWrits[path[p]]);
                }
                allChains.push(Ksee::Move(chain));
                return;
            }
            for (usz ch = 0; ch < childrenOf[curr].size(); ++ch) {
                usz nextIdx = childrenOf[curr][ch];
                if (!visited[nextIdx]) {
                    visited[nextIdx] = true;
                    path.push(nextIdx);
                    dfs(nextIdx, target, path, visited);
                    path.pop();
                    visited[nextIdx] = false;
                }
            }
        };

    for (usz l = 0; l < candidateLeaves.size(); ++l) {
        usz leaf = candidateLeaves[l];
        for (usz r = 0; r < n; ++r) {
            if (!isRootTrusted[r]) continue;
            Array<usz> path;
            Array<bool> visited;
            visited.allocate(n);
            for (usz k = 0; k < n; ++k) visited[k] = false;
            visited[r] = true;
            path.push(r);
            dfs(r, leaf, path, visited);
        }
    }

    return allChains;
}

struct NodeRule {
    Path relPath;
    Tree<void>* node;
};

static void collectLeafRules(
    const Tree<void>* curr,
    Path currentRel,
    Array<NodeRule> &out
) {
    for (usz i = 0; i < curr->children.size(); ++i) {
        Tree<void>* ch = curr->children[i];
        Path nextRel = currentRel.isEmpty() ? ch->path : Path(currentRel.toString() + "/" + ch->path.toString());
        if (ch->hasValue() || ch->valueType() != 0) {
            out.push({nextRel, ch});
        }
        collectLeafRules(ch, nextRel, out);
    }
}

Tree<void> Writ::mask(const Sparse<Tree<void>> &chain) {
    Tree<void> result;
    if (chain.size() == 0) return result;

    Tree<void>* firstPerm = chain[0].get("perm");
    if (firstPerm) {
        result = *firstPerm;
    }
    result.path = Path("perm");

    for (usz step = 1; step < chain.size(); ++step) {
        Tree<void>* childPerm = chain[step].get("perm");
        if (!childPerm) continue;

        Array<NodeRule> childRules;
        collectLeafRules(childPerm, Path(), childRules);

        for (usz r = 0; r < childRules.size(); ++r) {
            const Path &p = childRules[r].relPath;
            Tree<void>* chNode = childRules[r].node;

            Tree<void>* parentMatch = result.query(p);
            if (parentMatch) {
                if (chNode->valueType() == 2 || chNode->valueType() == 3 ||
                    dynamic_cast<const Tree<long long>*>(chNode) ||
                    dynamic_cast<const Tree<int>*>(chNode)) {
                    f64 pVal = parentMatch->asNumber();
                    f64 cVal = chNode->asNumber();
                    f64 eff = (cVal < pVal) ? cVal : pVal;
                    result[p] = (long long)eff;
                } else {
                    result[p] = chNode->asString();
                }
            } else {
                if (result.children.size() == 0) {
                    if (chNode->valueType() == 2 || chNode->valueType() == 3 ||
                        dynamic_cast<const Tree<long long>*>(chNode) ||
                        dynamic_cast<const Tree<int>*>(chNode)) {
                        result[p] = (long long)chNode->asNumber();
                    } else {
                        result[p] = chNode->asString();
                    }
                } else {
                    result[p] = (long long)0;
                }
            }
        }
    }

    return result;
}

} // namespace Ksee
