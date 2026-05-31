#pragma once

#include "pch/Pch.h"

class SphereCollider : public Collider {
public:
    float radius;    
    SphereCollider(float r = 0.5f);    
    SphereCollider(float r, SM::Vector3 c);    
    JPH::Ref<JPH::Shape> CreateShape() override;    
    json Serialize() override;    
    void Deserialize(const json& j) override;    
    void OnShapeGUI(bool& shapeChanged) override;
};


