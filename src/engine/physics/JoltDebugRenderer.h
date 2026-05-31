#pragma once

#include "pch/Pch.h"

struct DebugLine {
    SM::Vector3 from;
    SM::Vector3 to;
    uint32_t color;
};

class JoltDebugRenderer final : public JPH::DebugRendererSimple {
public:
    std::vector<DebugLine> m_lines;    
    JoltDebugRenderer();    
    virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;    
    virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override;    
    virtual void DrawText3D(JPH::RVec3Arg inPos, const std::string_view& inStr, JPH::ColorArg inCol, float inH) override;
};

