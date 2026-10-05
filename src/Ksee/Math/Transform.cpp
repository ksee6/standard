/**
 * @file Transform.cpp
 * @brief Spatial transforms and geodesy implementation for Ksee.
 */

#include "../../../include/Ksee/Math/Transform.hpp"

namespace Ksee {

Matrix4 Transform::getMatrix() const {
    if (_cachedVersion == transformVersion) {
        return _cachedMatrix;
    }

    Matrix4 t = Matrix4::translation(position);
    Matrix4 rx = Matrix4::rotationX(rotation.x);
    Matrix4 ry = Matrix4::rotationY(rotation.y);
    Matrix4 rz = Matrix4::rotationZ(rotation.z);
    Matrix4 s = Matrix4::scale(scale);

    _cachedMatrix = t * (rz * ry * rx) * s;
    _cachedVersion = transformVersion;
    return _cachedMatrix;
}

void Transform::lookAt(Vector3 target, Vector3 up) {
    Vector3 dir = (target - position).normalized();
    Vector3 right = dir.cross(up).normalized();
    Vector3 realUp = right.cross(dir).normalized();

    f32 pitch = asin(-dir.y);
    f32 yaw = atan2(dir.x, dir.z);
    f32 roll = atan2(right.y, realUp.y);

    rotation = {pitch, yaw, roll};
    update();
}

void GeoSpatial::setGeoPos(Vector3 lla) {
    f64 lat = lla.x * (PI / 180.0);
    f64 lon = lla.y * (PI / 180.0);
    f64 alt = lla.z;

    f64 sinLat = sin((f32)lat);
    f64 cosLat = cos((f32)lat);
    f64 N = SphereConfig::WGS84_A /
            __builtin_sqrt(1.0 - SphereConfig::WGS84_E2 * sinLat * sinLat);

    position.x = (f32)((N + alt) * cosLat * cos((f32)lon));
    position.y = (f32)((N + alt) * cosLat * sin((f32)lon));
    position.z = (f32)((N * (1.0 - SphereConfig::WGS84_E2) + alt) * sinLat);
    update();
}

Vector3 GeoSpatial::getGeoPosition() const {
    f64 x = position.x;
    f64 y = position.y;
    f64 z = position.z;

    f64 lon = atan2((f32)y, (f32)x);
    f64 p = __builtin_sqrt(x * x + y * y);
    f64 lat = atan2((f32)z, (f32)(p * (1.0 - SphereConfig::WGS84_E2)));
    f64 h = 0;

    for (int i = 0; i < 5; ++i) {
        f64 sinLat = sin((f32)lat);
        f64 N = SphereConfig::WGS84_A /
                __builtin_sqrt(1.0 - SphereConfig::WGS84_E2 * sinLat * sinLat);
        h = p / cos((f32)lat) - N;
        lat = atan2((f32)z, (f32)(p * (1.0 - SphereConfig::WGS84_E2 * N / (N + h))));
    }

    return {(f32)(lat * (180.0 / PI)), (f32)(lon * (180.0 / PI)), (f32)h};
}

int GeoSpatial::getGMT() const {
    Vector3 lla = getGeoPosition();
    return (int)((lla.y / 15.0f) + 0.5f) * 3600;
}

} // namespace Ksee

