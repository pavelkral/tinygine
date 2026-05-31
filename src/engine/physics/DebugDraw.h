#pragma once

#include "pch/Pch.h"

class DebugDraw {
private:
    static std::vector<Vertex>* s_vertices;
public:    
    static void Begin(std::vector<Vertex>& vertices);    
    static void DrawLine(const SM::Vector3& p1, const SM::Vector3& p2, const SM::Vector3& color);    
    static void DrawBox(const SM::Vector3& pos, const SM::Quaternion& rot, const SM::Vector3& extents, const SM::Vector3& color);    
    static void DrawCircle(const SM::Vector3& pos, const SM::Quaternion& rot, float radius, int segments, const SM::Vector3& color);    
    static void DrawSphere(const SM::Vector3& pos, const SM::Quaternion& rot, float radius, const SM::Vector3& color);    
    static void DrawCapsule(const SM::Vector3& pos, const SM::Quaternion& rot, float halfHeight, float radius, const SM::Vector3& color);
};


