/**
 * @file Router.hpp
 * @brief High-performance hierarchical Radix Tree Router for Ksee with full lifecycle hooks.
 */

#ifndef KSEE_RESOURCE_ROUTER_HPP
#define KSEE_RESOURCE_ROUTER_HPP

#include "../Tree.hpp"
#include "../Path.hpp"
#include "../Func.hpp"
#include "../Array.hpp"
#include "../String.hpp"

namespace Ksee {
namespace Resource {

template <typename T>
class Router : public Tree<void> {
public:
    using Handler = Func<bool(Path&, T&)>;
    using LeavingHook = Func<bool(Path&, T&)>;
    using ResolvedHook = Func<void(Path&, T&)>;
    using DoneHook = Func<void()>;

    struct RouteEntry {
        String method; // Empty for any method, or "GET", "POST", "PUT", "DELETE"
        Handler handler;
    };

    Path currentPath;
    T currentRequest;
    Array<Path> history;

private:
    Array<LeavingHook> _leavingHooks;
    Array<ResolvedHook> _resolvedHooks;
    Array<DoneHook> _doneHooks;

    void addRoute(const Path &path, const String &method, Handler handler) {
        Path target = path;
        if (target.isEmpty()) {
            target = Path("/");
        }

        RouteEntry entry;
        entry.method = method;
        entry.handler = handler;

        Tree<void>* existing = this->get(target);
        if (existing) {
            auto* typed = dynamic_cast<Tree<Array<RouteEntry>>*>(existing);
            if (typed) {
                typed->value.push(entry);
                return;
            }
        }

        Array<RouteEntry> list;
        list.push(entry);
        this->template push<Array<RouteEntry>>(target, list);
    }

    static int scoreNode(const Tree<void>* node) {
        int score = 0;
        for (usz k = 0; k < node->path.parts.size(); ++k) {
            const String &pt = node->path.parts[k];
            if (pt == "**") score += 1;
            else if (pt == "*" || pt.isEmpty()) score += 5;
            else score += 20; // Exact segments have highest specificity
        }
        return score;
    }

    bool dispatchHandlers(Path &reqPath, T &reqObj, usz depth = 0) {
        if (depth > 16) return false; // Prevent recursion loops

        Path lookupPath = reqPath;
        if (reqPath.isUrl()) {
            lookupPath = reqPath.path();
            if (lookupPath.isEmpty()) {
                lookupPath = reqPath.base();
            }
        }
        if (lookupPath.isEmpty()) lookupPath = Path("/");
        Array<Tree<void>*> candidates = this->queryAll(lookupPath);
        Tree<void>* exactNode = this->get(lookupPath);
        if (exactNode) {
            bool found = false;
            for (usz i = 0; i < candidates.size(); ++i) {
                if (candidates[i] == exactNode) { found = true; break; }
            }
            if (!found) candidates.push(exactNode);
        }
        if (candidates.isEmpty()) {
            Tree<void>* rootNode = this->get(Path("/"));
            if (rootNode) candidates.push(rootNode);
        }

        // Sort candidates by specificity score (descending)
        for (usz i = 1; i < candidates.size(); ++i) {
            Tree<void>* key = candidates[i];
            int keyScore = scoreNode(key);
            long long j = (long long)i - 1;
            while (j >= 0 && scoreNode(candidates[(usz)j]) < keyScore) {
                candidates[(usz)(j + 1)] = candidates[(usz)j];
                j--;
            }
            candidates[(usz)(j + 1)] = key;
        }

        String reqMethod = reqPath.getMethod();
        if (reqMethod.isEmpty()) reqMethod = "GET";

        for (usz i = 0; i < candidates.size(); ++i) {
            auto* typed = dynamic_cast<Tree<Array<RouteEntry>>*>(candidates[i]);
            if (!typed) continue;

            for (usz k = 0; k < typed->value.size(); ++k) {
                RouteEntry &entry = typed->value[k];
                if (!entry.method.isEmpty() && entry.method != reqMethod) {
                    continue;
                }

                Path beforeCall = reqPath;
                bool handled = entry.handler(reqPath, reqObj);

                if (handled) {
                    return true;
                }

                // Handler returned false: check if it altered requestedPath
                if (reqPath.isBack()) {
                    if (history.size() > 0) {
                        reqPath = history.pop();
                    }
                    return dispatchHandlers(reqPath, reqObj, depth + 1);
                }

                if (reqPath != beforeCall) {
                    // Path was modified, re-route with the updated path
                    return dispatchHandlers(reqPath, reqObj, depth + 1);
                }
            }
        }

        return false;
    }

public:
    using Tree<void>::get;

    Router() : Tree<void>(), currentPath(), currentRequest() {}
    explicit Router(const Path &rootPath) : Tree<void>(rootPath), currentPath(), currentRequest() {}

    // Handler registrations
    void handle(const Path &path, Handler handler) {
        addRoute(path, "", handler);
    }
    void handle(const String &path, Handler handler) {
        handle(Path(path), handler);
    }
    void handle(const char *path, Handler handler) {
        handle(Path(path), handler);
    }

    void get(const Path &path, Handler handler) {
        addRoute(path, "GET", handler);
    }
    void get(const String &path, Handler handler) {
        get(Path(path), handler);
    }
    void get(const char *path, Handler handler) {
        get(Path(path), handler);
    }

    void post(const Path &path, Handler handler) {
        addRoute(path, "POST", handler);
    }
    void post(const String &path, Handler handler) {
        post(Path(path), handler);
    }
    void post(const char *path, Handler handler) {
        post(Path(path), handler);
    }

    void put(const Path &path, Handler handler) {
        addRoute(path, "PUT", handler);
    }
    void put(const String &path, Handler handler) {
        put(Path(path), handler);
    }
    void put(const char *path, Handler handler) {
        put(Path(path), handler);
    }

    void del(const Path &path, Handler handler) {
        addRoute(path, "DELETE", handler);
    }
    void del(const String &path, Handler handler) {
        del(Path(path), handler);
    }
    void del(const char *path, Handler handler) {
        del(Path(path), handler);
    }

    void delete_(const Path &path, Handler handler) { del(path, handler); }
    void delete_(const String &path, Handler handler) { del(path, handler); }
    void delete_(const char *path, Handler handler) { del(path, handler); }

    // Lifecycle hooks
    void onLeaving(LeavingHook hook) {
        _leavingHooks.push(hook);
    }

    void onResolved(ResolvedHook hook) {
        _resolvedHooks.push(hook);
    }

    void onDone(DoneHook hook) {
        _doneHooks.push(hook);
    }

    // Execution / Visit pipeline
    bool visit(Path path, T request = T()) {
        usz redirects = 0;
        const usz MAX_REDIRECTS = 32;

        Path target = path;
        T req = request;

        while (redirects++ < MAX_REDIRECTS) {
            // Handle Back navigation
            if (target.isBack()) {
                if (history.size() > 0) {
                    target = history.pop();
                } else {
                    return false;
                }
            }

            Path cycleStart = target;

            // 1. onLeaving Phase
            bool allowLeave = true;
            for (usz i = 0; i < _leavingHooks.size(); ++i) {
                if (!_leavingHooks[i](target, req)) {
                    allowLeave = false;
                    break;
                }
                if (target.isBack()) {
                    allowLeave = false;
                    break;
                }
            }

            if (!allowLeave) {
                return false;
            }

            if (target != cycleStart) {
                continue; // Redirected during onLeaving
            }

            // 2. Handle Phase
            bool handled = dispatchHandlers(target, req);

            // 3. onResolved Phase
            Path afterResolved = target;
            for (usz i = 0; i < _resolvedHooks.size(); ++i) {
                _resolvedHooks[i](afterResolved, req);
            }

            // If onResolved modified target, trigger another visit cycle
            if (afterResolved != target || afterResolved.isBack()) {
                target = afterResolved;
                continue;
            }

            // Commit current state: after all resolved, and before done
            currentPath = target;
            currentRequest = req;

            // Record into history
            if (history.isEmpty() || history[history.size() - 1] != currentPath) {
                history.push(currentPath);
            }

            // 4. onDone Phase
            for (usz i = 0; i < _doneHooks.size(); ++i) {
                _doneHooks[i]();
            }

            return handled;
        }

        return false; // Max redirects exceeded
    }

    bool visit(const String &path, T request = T()) {
        return visit(Path(path), request);
    }
    bool visit(const char *path, T request = T()) {
        return visit(Path(path), request);
    }

    bool back() {
        if (history.size() <= 1) return false;
        history.pop(); // Pop current
        Path prev = history.pop();
        return visit(prev, currentRequest);
    }

    bool operator()(const Path &path, T request = T()) {
        return visit(path, request);
    }
    bool operator()(const String &path, T request = T()) {
        return visit(Path(path), request);
    }
    bool operator()(const char *path, T request = T()) {
        return visit(Path(path), request);
    }
};

} // namespace Resource
} // namespace Ksee

#endif // KSEE_RESOURCE_ROUTER_HPP
