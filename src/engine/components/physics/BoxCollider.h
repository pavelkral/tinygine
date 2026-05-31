#pragma once

#include "pch/Pch.h"

class BoxCollider : public Collider {
public:
    SM::Vector3 size;    
    BoxCollider(SM::Vector3 s);    
    BoxCollider(SM::Vector3 s, SM::Vector3 c);    
    JPH::Ref<JPH::Shape> CreateShape() override;    
    json Serialize() override;    
    void Deserialize(const json& j) override;    
    void OnShapeGUI(bool& shapeChanged) override;
};

