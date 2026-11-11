/**
 * @file Set.hpp
 * @brief High-performance Invertible Mathematical Set backed by Map<T, bool>.
 */

#ifndef KSEE_SET_HPP
#define KSEE_SET_HPP

#include "Map.hpp"

namespace Ksee {

template <typename T>
class Set {
public:
    Map<T, bool> map;
    bool isInversed = false;

    Set() : isInversed(false) {}

    bool has(const T &val) const {
        bool found = map.has(val);
        return isInversed ? !found : found;
    }

    bool includes(const T &val) const {
        return has(val);
    }

    bool operator()(const T &val) const {
        return has(val);
    }

    void add(const T &val) {
        if (!isInversed) {
            map.put(val, true);
        } else {
            // In an inverted set (universal complement / blacklist),
            // adding an element to the set means removing it from the blacklist.
            map.remove(val);
        }
    }

    void del(const T &val) {
        if (!isInversed) {
            map.remove(val);
        } else {
            // Removing an element from an inverted set means adding it to the blacklist.
            map.put(val, true);
        }
    }

    void remove(const T &val) {
        del(val);
    }

    usz size() const {
        return map.size();
    }

    usz length() const {
        return size();
    }

    bool isEmpty() const {
        return map.isEmpty() && !isInversed;
    }

    void clear() {
        map.clear();
        isInversed = false;
    }

    Set<T> bar() const {
        Set<T> res = *this;
        res.isInversed = !res.isInversed;
        return res;
    }

    Set<T> operator~() const {
        return bar();
    }

    Set<T> operator&(const Set<T> &o) const {
        if (!isInversed && !o.isInversed) {
            Set<T> res;
            const Set<T> &smaller = (size() <= o.size()) ? *this : o;
            const Set<T> &larger = (size() <= o.size()) ? o : *this;
            for (auto &entry : smaller.map) {
                if (larger.has(entry.key))
                    res.add(entry.key);
            }
            return res;
        }
        if (isInversed && o.isInversed) {
            // ~A & ~B = ~(A | B)
            Set<T> res;
            res.isInversed = true;
            for (auto &e : map) res.map.put(e.key, true);
            for (auto &e : o.map) res.map.put(e.key, true);
            return res;
        }
        if (!isInversed && o.isInversed) {
            // A & ~B = A \ B
            Set<T> res;
            for (auto &e : map) {
                if (o.has(e.key))
                    res.add(e.key);
            }
            return res;
        }
        // ~A & B = B \ A
        Set<T> res;
        for (auto &e : o.map) {
            if (has(e.key))
                res.add(e.key);
        }
        return res;
    }

    Set<T> operator|(const Set<T> &o) const {
        if (!isInversed && !o.isInversed) {
            Set<T> res = *this;
            for (auto &entry : o.map) {
                res.add(entry.key);
            }
            return res;
        }
        if (isInversed && o.isInversed) {
            // ~A | ~B = ~(A & B)
            Set<T> res;
            res.isInversed = true;
            for (auto &e : map) {
                if (o.map.has(e.key))
                    res.map.put(e.key, true);
            }
            return res;
        }
        if (isInversed && !o.isInversed) {
            Set<T> res = *this;
            for (auto &e : o.map) {
                res.add(e.key);
            }
            return res;
        }
        Set<T> res = o;
        for (auto &e : map) {
            res.add(e.key);
        }
        return res;
    }

    Set<T> operator-(const Set<T> &o) const {
        return (*this) & (~o);
    }

    Array<T> toArray() const {
        return map.keys();
    }

    // Iterator support for range-based for loops
    struct Iterator {
        typename Map<T, bool>::Iterator it;
        Iterator(typename Map<T, bool>::Iterator i) : it(i) {}
        bool operator!=(const Iterator &o) const { return it != o.it; }
        Iterator &operator++() { ++it; return *this; }
        const T &operator*() const { return it->key; }
    };

    struct ConstIterator {
        typename Map<T, bool>::ConstIterator it;
        ConstIterator(typename Map<T, bool>::ConstIterator i) : it(i) {}
        bool operator!=(const ConstIterator &o) const { return it != o.it; }
        ConstIterator &operator++() { ++it; return *this; }
        const T &operator*() const { return it->key; }
    };

    Iterator begin() { return Iterator(map.begin()); }
    Iterator end() { return Iterator(map.end()); }
    ConstIterator begin() const { return ConstIterator(map.begin()); }
    ConstIterator end() const { return ConstIterator(map.end()); }
};

} // namespace Ksee

#endif // KSEE_SET_HPP
