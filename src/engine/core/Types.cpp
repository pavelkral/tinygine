#include "engine/EngineDependencies.h"
#include "engine/core/Types.h"

SM::Matrix AssimpToSimpleMathMatrix(const aiMatrix4x4& m) {
    SM::Matrix mat(
        m.a1, m.a2, m.a3, m.a4,
        m.b1, m.b2, m.b3, m.b4,
        m.c1, m.c2, m.c3, m.c4,
        m.d1, m.d2, m.d3, m.d4);
    return mat.Transpose();
}
