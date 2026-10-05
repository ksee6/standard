/**
 * @file Trig.hpp
 * @brief High-performance scalar and SIMD/struct trigonometric functions for Ksee.
 */

#ifndef KSEE_MATH_TRIG_HPP
#define KSEE_MATH_TRIG_HPP

#include "../Types.hpp"

namespace Ksee {

static constexpr f32 TWO_PI = 6.28318530717958647692f;
static constexpr f32 HALF_PI = 1.57079632679489661923f;

inline f32 degToRad(f32 deg) { return deg * (PI / 180.0f); }
inline f32 radToDeg(f32 rad) { return rad * (180.0f / PI); }

#if defined(_MSC_VER) && !defined(__clang__)
#include <math.h>
inline f32 sin(f32 x) { return ::sinf(x); }
inline f32 cos(f32 x) { return ::cosf(x); }
inline f32 tan(f32 x) { return ::tanf(x); }
inline f32 asin(f32 x) { return ::asinf(x); }
inline f32 acos(f32 x) { return ::acosf(x); }
inline f32 atan(f32 x) { return ::atanf(x); }
inline f32 atan2(f32 y, f32 x) { return ::atan2f(y, x); }
inline f32 sinh(f32 x) { return ::sinhf(x); }
inline f32 cosh(f32 x) { return ::coshf(x); }
inline f32 tanh(f32 x) { return ::tanhf(x); }
inline f32 asinh(f32 x) { return ::asinhf(x); }
inline f32 acosh(f32 x) { return ::acoshf(x); }
inline f32 atanh(f32 x) { return ::atanhf(x); }
#else
inline f32 sin(f32 x) { return __builtin_sinf(x); }
inline f32 cos(f32 x) { return __builtin_cosf(x); }
inline f32 tan(f32 x) { return __builtin_tanf(x); }
inline f32 asin(f32 x) { return __builtin_asinf(x); }
inline f32 acos(f32 x) { return __builtin_acosf(x); }
inline f32 atan(f32 x) { return __builtin_atanf(x); }
inline f32 atan2(f32 y, f32 x) { return __builtin_atan2f(y, x); }
inline f32 sinh(f32 x) { return __builtin_sinhf(x); }
inline f32 cosh(f32 x) { return __builtin_coshf(x); }
inline f32 tanh(f32 x) { return __builtin_tanhf(x); }
inline f32 asinh(f32 x) { return __builtin_asinhf(x); }
inline f32 acosh(f32 x) { return __builtin_acoshf(x); }
inline f32 atanh(f32 x) { return __builtin_atanhf(x); }
#endif

// Generic vector/struct overloads
#define KSEE_MATH_TRI_OVERLOAD(name) \
  template <typename T> inline T name(const T &v) { \
    T res = v; \
    f32 *pr = reinterpret_cast<f32 *>(&res); \
    const f32 *pv = reinterpret_cast<const f32 *>(&v); \
    for (usz i = 0; i < sizeof(T) / sizeof(f32); ++i) \
      pr[i] = ::Ksee::name(pv[i]); \
    return res; \
  }

KSEE_MATH_TRI_OVERLOAD(sin)
KSEE_MATH_TRI_OVERLOAD(cos)
KSEE_MATH_TRI_OVERLOAD(tan)
KSEE_MATH_TRI_OVERLOAD(asin)
KSEE_MATH_TRI_OVERLOAD(acos)
KSEE_MATH_TRI_OVERLOAD(atan)
KSEE_MATH_TRI_OVERLOAD(sinh)
KSEE_MATH_TRI_OVERLOAD(cosh)
KSEE_MATH_TRI_OVERLOAD(tanh)
KSEE_MATH_TRI_OVERLOAD(asinh)
KSEE_MATH_TRI_OVERLOAD(acosh)
KSEE_MATH_TRI_OVERLOAD(atanh)

#undef KSEE_MATH_TRI_OVERLOAD

} // namespace Ksee

#endif // KSEE_MATH_TRIG_HPP

