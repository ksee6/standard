/**
 * @file Matrix.hpp
 * @brief High-performance 4x4 matrix math for graphics and spatial transforms in Ksee.
 */

#ifndef KSEE_MATH_MATRIX_HPP
#define KSEE_MATH_MATRIX_HPP

#include "../Types.hpp"
#include "Trig.hpp"
#include "Vec.hpp"

namespace Ksee {

struct Matrix4 {
    f32 m[4][4]{};

    Matrix4() {
        m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
    }

    f32 at(int r, int c) const { return m[r][c]; }
    f32& at(int r, int c) { return m[r][c]; }
    f32 operator()(int r, int c) const { return m[r][c]; }
    f32& operator()(int r, int c) { return m[r][c]; }

    static Matrix4 identity() {
        return Matrix4();
    }

    static Matrix4 translation(f32 x, f32 y, f32 z) {
        Matrix4 r;
        r.m[0][3] = x;
        r.m[1][3] = y;
        r.m[2][3] = z;
        return r;
    }

    static Matrix4 translation(const Vector3 &v) {
        return translation(v.x, v.y, v.z);
    }

    static Matrix4 scale(f32 x, f32 y, f32 z) {
        Matrix4 r;
        r.m[0][0] = x;
        r.m[1][1] = y;
        r.m[2][2] = z;
        return r;
    }

    static Matrix4 scale(const Vector3 &v) {
        return scale(v.x, v.y, v.z);
    }

    static Matrix4 rotationX(f32 rad) {
        Matrix4 r;
        f32 c = cos(rad), s = sin(rad);
        r.m[1][1] = c; r.m[1][2] = -s;
        r.m[2][1] = s; r.m[2][2] = c;
        return r;
    }

    static Matrix4 rotationY(f32 rad) {
        Matrix4 r;
        f32 c = cos(rad), s = sin(rad);
        r.m[0][0] = c;  r.m[0][2] = s;
        r.m[2][0] = -s; r.m[2][2] = c;
        return r;
    }

    static Matrix4 rotationZ(f32 rad) {
        Matrix4 r;
        f32 c = cos(rad), s = sin(rad);
        r.m[0][0] = c; r.m[0][1] = -s;
        r.m[1][0] = s; r.m[1][1] = c;
        return r;
    }

    Matrix4 operator*(const Matrix4 &o) const {
        Matrix4 r;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                r.m[i][j] = m[i][0] * o.m[0][j] +
                            m[i][1] * o.m[1][j] +
                            m[i][2] * o.m[2][j] +
                            m[i][3] * o.m[3][j];
            }
        }
        return r;
    }

    Vector4 operator*(const Vector4 &v) const {
        return {
            m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z + m[0][3]*v.w,
            m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z + m[1][3]*v.w,
            m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z + m[2][3]*v.w,
            m[3][0]*v.x + m[3][1]*v.y + m[3][2]*v.z + m[3][3]*v.w
        };
    }

    Vector3 transformPoint(const Vector3 &p) const {
        Vector4 res = (*this) * Vector4(p, 1.0f);
        if (res.w != 0.0f && res.w != 1.0f) {
            return {res.x / res.w, res.y / res.w, res.z / res.w};
        }
        return {res.x, res.y, res.z};
    }

    Matrix4 transpose() const {
        Matrix4 r;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                r.m[i][j] = m[j][i];
            }
        }
        return r;
    }
};

} // namespace Ksee

#endif // KSEE_MATH_MATRIX_HPP

