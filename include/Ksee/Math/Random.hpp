/**
 * @file Random.hpp
 * @brief Pseudo-random generation extended for all number types in Ksee.
 */

#ifndef KSEE_MATH_RANDOM_HPP
#define KSEE_MATH_RANDOM_HPP

#include "../Types.hpp"

namespace Ksee {

inline f32 randomFloat() {
    return (f32)random(0xFFFFFFFFU) / 4294967295.0f;
}

inline f64 randomDouble() {
    u64 r = ((u64)random(0xFFFFFFFFU) << 32) | (u64)random(0xFFFFFFFFU);
    return (f64)r / 18446744073709551615.0;
}

template <typename T>
inline T randomNumber() {
    if constexpr (IsSame<T, f32>::Value) {
        return randomFloat();
    } else if constexpr (IsSame<T, f64>::Value) {
        return randomDouble();
    } else if constexpr (sizeof(T) <= 4) {
        return (T)random();
    } else if constexpr (sizeof(T) == 8) {
        return (T)(((u64)random() << 32) | (u64)random());
    } else if constexpr (sizeof(T) == 16) {
        u64 low = ((u64)random() << 32) | (u64)random();
        u64 high = ((u64)random() << 32) | (u64)random();
        if constexpr (IsSame<T, u128>::Value) {
            return u128(high, low);
        } else {
#if defined(__SIZEOF_INT128__)
            return (T)(((unsigned __int128)high << 64) | (unsigned __int128)low);
#else
            return (T)low;
#endif
        }
    } else {
        return (T)random();
    }
}

template <typename T>
inline T randomNumber(T minVal, T maxVal) {
    if (minVal >= maxVal) return minVal;
    if constexpr (IsSame<T, f32>::Value) {
        return minVal + (maxVal - minVal) * randomFloat();
    } else if constexpr (IsSame<T, f64>::Value) {
        return minVal + (maxVal - minVal) * randomDouble();
    } else {
        u64 range = (u64)(maxVal - minVal);
        return minVal + (T)(randomNumber<u64>() % (range + 1));
    }
}

} // namespace Ksee

#endif // KSEE_MATH_RANDOM_HPP

