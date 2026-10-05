/**
 * @file Transform.hpp
 * @brief Spatial transforms, geodesy, and positioning for Ksee.
 */

#ifndef KSEE_MATH_TRANSFORM_HPP
#define KSEE_MATH_TRANSFORM_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "Trig.hpp"
#include "Vec.hpp"
#include "Matrix.hpp"
#include "Quaternion.hpp"

namespace Ksee {

struct Transform {
    virtual ~Transform() = default;

    Vector3 position = {0, 0, 0};
    Vector3 rotation = {0, 0, 0}; // Euler angles in radians
    Vector3 scale = {1, 1, 1};

    u32 transformVersion = 1;

    void update() {
        transformVersion++;
        if (transformVersion == 0) transformVersion = 1;
    }

    Matrix4 getMatrix() const;
    void lookAt(Vector3 target, Vector3 up = {0, 0, 1});

    mutable Matrix4 _cachedMatrix;
    mutable u32 _cachedVersion = 0;
};

struct SphereConfig {
    static constexpr f64 WGS84_A = 6378137.0;
    static constexpr f64 WGS84_F = 1.0 / 298.257223563;
    static constexpr f64 WGS84_B = WGS84_A * (1.0 - WGS84_F);
    static constexpr f64 WGS84_E2 = (WGS84_A * WGS84_A - WGS84_B * WGS84_B) / (WGS84_A * WGS84_A);
};

class GeoSpatial : public Transform {
public:
    void setGeoPos(Vector3 lla);
    Vector3 getGeoPosition() const;
    int getGMT() const;
};

} // namespace Ksee

#endif // KSEE_MATH_TRANSFORM_HPP

