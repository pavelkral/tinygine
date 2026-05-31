#include "engine/EngineDependencies.h"
#include "engine/scene/SceneTypes.h"

Vector3d g_physicsOrigin;

Vector3d::Vector3d(double X, double Y, double Z) : x(X), y(Y), z(Z) {}

Vector3d Vector3d::operator+(const Vector3d& other) const {
    return { x + other.x, y + other.y, z + other.z };
}

Vector3d Vector3d::operator-(const Vector3d& other) const {
    return { x - other.x, y - other.y, z - other.z };
}

Vector3d Vector3d::operator*(double scalar) const {
    return { x * scalar, y * scalar, z * scalar };
}
