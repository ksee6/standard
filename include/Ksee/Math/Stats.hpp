/**
 * @file Stats.hpp
 * @brief Statistical operations (sum, mean, variance, standard deviation) for Ksee.
 */

#ifndef KSEE_MATH_STATS_HPP
#define KSEE_MATH_STATS_HPP

#include "../Types.hpp"
#include "../Array.hpp"

namespace Ksee {

template <typename T> inline f32 sum(const T &v) {
  const f32 *p = reinterpret_cast<const f32 *>(&v);
  f32 s = 0;
  for (usz i = 0; i < sizeof(T) / sizeof(f32); ++i)
    s += p[i];
  return s;
}

template <typename T> inline f32 mean(const T &v) {
  return sum(v) / (f32)(sizeof(T) / sizeof(f32));
}

template <typename T> inline f32 sum(const Array<T> &a) {
  f32 s = 0;
  usz n = a.size();
  const T *d = a.data();
  for (usz i = 0; i < n; ++i) s += (f32)d[i];
  return s;
}

template <typename T> inline f32 mean(const Array<T> &a) {
  usz n = a.size();
  return (n == 0) ? 0.0f : sum(a) / (f32)n;
}

template <typename T> inline f32 variance(const Array<T> &a) {
  usz n = a.size();
  if (n == 0) return 0.0f;
  f32 m = mean(a);
  f32 v = 0;
  const T *d = a.data();
  for (usz i = 0; i < n; ++i) {
    f32 diff = (f32)d[i] - m;
    v += diff * diff;
  }
  return v / (f32)n;
}

template <typename T> inline f32 stddev(const Array<T> &a) {
  return __builtin_sqrtf(variance(a));
}

} // namespace Ksee

#endif // KSEE_MATH_STATS_HPP

