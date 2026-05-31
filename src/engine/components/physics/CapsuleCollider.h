#pragma once

#include "pch/Pch.h"

class CapsuleCollider : public Collider {
public:
    float halfHeight, radius;    
    CapsuleCollider(float hh = 0.5f, float r = 0.5f);    
    CapsuleCollider(float hh, float r, SM::Vector3 c);    
    JPH::Ref<JPH::Shape> CreateShape() override;    
    json Serialize() override;    
    void Deserialize(const json& j) override;    
    void OnShapeGUI(bool& shapeChanged) override;

};


