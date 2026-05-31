#pragma once

#include "pch/Pch.h"

class Rigidbody;
// Base Collider
class Collider : public Component {
public:
    SM::Vector3 center = { 0.0f, 0.0f, 0.0f }; // OFFSET!

    virtual JPH::Ref<JPH::Shape> CreateShape() = 0;
    void OnGUI() override;    
    virtual void OnShapeGUI(bool& shapeChanged);

protected:    
    JPH::Ref<JPH::Shape> ApplyOffset(JPH::Ref<JPH::Shape> baseShape);
};

