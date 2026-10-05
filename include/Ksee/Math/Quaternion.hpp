/**
 * @file Quaternion.hpp
 * @brief High-performance Quaternion mathematics for 3D rotations in Ksee.
 */

#ifndef KSEE_MATH_QUATERNION_HPP
#define KSEE_MATH_QUATERNION_HPP

#include "../Types.hpp"
#include "Trig.hpp"
#include "Vec.hpp"
#include "Matrix.hpp"

namespace Ksee {

struct Quaternion {
    f32 x = 0, y = 0, z = 0, w = 1.0f;

    Quaternion() = default;
    Quaternion(f32 x_, f32 y_, f32 z_, f32 w_) : x(x_), y(y_), z(z_), w(w_) {}

    static Quaternion identity() {
        return {0, 0, 0, 1.0f};
    }

    static Quaternion fromAxisAngle(const Vector3 &axis, f32 rad) {
        f32 half = rad * 0.5f;
        f32 s = sin(half);
        Vector3 norm = axis.normalized();
        return {norm.x * s, norm.y * s, norm.z * s, cos(half)};
    }

    static Quaternion fromEuler(f32 pitch, f32 yaw, f32 roll) {
        f32 p = pitch * 0.5f, y = yaw * 0.5f, r = roll * 0.5f;
        f32 sinP = sin(p), cosP = cos(p);
        f32 sinY = sin(y), cosY = cos(y);
        f32 sinR = sin(r), cosR = cos(r);

        return {
            sinR * cosY * cosP - cosR * sinY * sinP,
            cosR * sinY * cosP + sinR * cosY * sinP,
            cosR * cosY * sinP - sinR * sinY * cosP,
            cosR * cosY * cosP + sinR * sinY * sinP
        };
    }

    Quaternion operator*(const Quaternion &o) const {
        return {
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
            w * o.w - x * o.x - y * o.y - z * o.z
        };
    }

    Quaternion conjugate() const {
        return {-x, -y, -z, w};
    }

    f32 lengthSquared() const {
        return x * x + y * y + z * z + w * w;
    }

    f32 length() const {
        return __builtin_sqrtf(lengthSquared());
    }

    Quaternion normalized() const {
        f32 len = length();
        if (len > 0.0f) {
            return {x / len, y / len, z / len, w / len};
        }
        return identity();
    }

    Matrix4 toMatrix4() const {
        Matrix4 m;
        f32 xx = x * x, yy = y * y, zz = z * z;
        f32 xy = x * y, xz = x * z, yz = y * z;
        f32 wx = w * x, wy = w * y, wz = w * z;

        m.m[0][0] = 1.0f - 2.0f * (yy + zz);
        m.m[0][1] = 2.0f * (xy - wz);
        m.m[0][2] = 2.0f * (xz + wy);

        m.m[1][0] = 2.0f * (xy + wz);
        m.m[1][1] = 1.0f - 2.0f * (xx + zz);
        m.m[1][2] = 2.0f * (yz - wx);

        m.m[2][0] = 2.0f * (xz - wy);
        m.m[2][1] = 2.0f * (yz + wx);
        m.m[2][2] = 1.0f - 2.0f * (xx + yy);

        return m;
    }
};

} // namespace Ksee

#endif // KSEE_MATH_QUATERNION_HPP

