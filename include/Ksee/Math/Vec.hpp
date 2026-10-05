/**
 * @file Vec.hpp
 * @brief High-performance 2D, 3D, and 4D vector mathematics for Ksee.
 */

#ifndef KSEE_MATH_VEC_HPP
#define KSEE_MATH_VEC_HPP

#include "../Types.hpp"
#include "Trig.hpp"

namespace Ksee {

struct Vector2 {
    f32 x = 0, y = 0;

    Vector2() = default;
    Vector2(f32 x_, f32 y_) : x(x_), y(y_) {}

    Vector2 operator+(const Vector2 &o) const { return {x + o.x, y + o.y}; }
    Vector2 operator-(const Vector2 &o) const { return {x - o.x, y - o.y}; }
    Vector2 operator*(f32 s) const { return {x * s, y * s}; }
    Vector2 operator/(f32 s) const { return {x / s, y / s}; }

    Vector2 &operator+=(const Vector2 &o) { x += o.x; y += o.y; return *this; }
    Vector2 &operator-=(const Vector2 &o) { x -= o.x; y -= o.y; return *this; }
    Vector2 &operator*=(f32 s) { x *= s; y *= s; return *this; }
    Vector2 &operator/=(f32 s) { x /= s; y /= s; return *this; }

    f32 dot(const Vector2 &o) const { return x * o.x + y * o.y; }
    f32 lengthSquared() const { return x * x + y * y; }
    f32 length() const { return __builtin_sqrtf(lengthSquared()); }

    Vector2 normalized() const {
        f32 len = length();
        return len > 0 ? (*this / len) : Vector2{0, 0};
    }
};

struct Vector3 {
    f32 x = 0, y = 0, z = 0;

    Vector3() = default;
    Vector3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}

    Vector3 operator+(const Vector3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(f32 s) const { return {x * s, y * s, z * s}; }
    Vector3 operator/(f32 s) const { return {x / s, y / s, z / s}; }

    Vector3 &operator+=(const Vector3 &o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vector3 &operator-=(const Vector3 &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vector3 &operator*=(f32 s) { x *= s; y *= s; z *= s; return *this; }
    Vector3 &operator/=(f32 s) { x /= s; y /= s; z /= s; return *this; }

    f32 dot(const Vector3 &o) const { return x * o.x + y * o.y + z * o.z; }
    Vector3 cross(const Vector3 &o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    f32 lengthSquared() const { return x * x + y * y + z * z; }
    f32 length() const { return __builtin_sqrtf(lengthSquared()); }

    Vector3 normalized() const {
        f32 len = length();
        return len > 0 ? (*this / len) : Vector3{0, 0, 0};
    }
};

struct Vector4 {
    f32 x = 0, y = 0, z = 0, w = 0;

    Vector4() = default;
    Vector4(f32 x_, f32 y_, f32 z_, f32 w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vector4(const Vector3 &v, f32 w_) : x(v.x), y(v.y), z(v.z), w(w_) {}

    Vector4 operator+(const Vector4 &o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Vector4 operator-(const Vector4 &o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    Vector4 operator*(f32 s) const { return {x * s, y * s, z * s, w * s}; }
    Vector4 operator/(f32 s) const { return {x / s, y / s, z / s, w / s}; }

    f32 dot(const Vector4 &o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    f32 lengthSquared() const { return x * x + y * y + z * z + w * w; }
    f32 length() const { return __builtin_sqrtf(lengthSquared()); }

    Vector4 normalized() const {
        f32 len = length();
        return len > 0 ? (*this / len) : Vector4{0, 0, 0, 0};
    }
};

inline f32 dot(const Vector2 &a, const Vector2 &b) { return a.dot(b); }
inline f32 dot(const Vector3 &a, const Vector3 &b) { return a.dot(b); }
inline f32 dot(const Vector4 &a, const Vector4 &b) { return a.dot(b); }
inline Vector3 cross(const Vector3 &a, const Vector3 &b) { return a.cross(b); }

using Vec2 = Vector2;
using Vec3 = Vector3;
using Vec4 = Vector4;

} // namespace Ksee

#endif // KSEE_MATH_VEC_HPP

