#pragma once

#include "pch/Pch.h"

struct Vector3d {
    double x;
    double y;
    double z;

    Vector3d(double X = 0.0, double Y = 0.0, double Z = 0.0);
    Vector3d operator+(const Vector3d& other) const;
    Vector3d operator-(const Vector3d& other) const;
    Vector3d operator*(double scalar) const;
};

extern Vector3d g_physicsOrigin;
