#pragma once

#include "pch/Pch.h"
#include "engine/components/Input.h"
#include "engine/scene/SceneTypes.h"

class FpsCamera {
public:
    XMFLOAT3 pos = { 0.0f, 2.0f, -20.0f };
    float yaw = 0.0f;
    float pitch = 0.0f;
    float speed = 10.0f;
    float sensitivity = 0.006f;    
    void Update(float dt);    
    XMMATRIX GetViewMatrix() const;    
    JPH::RRayCast GetMouseRay(float mouseX, float mouseY, float screenW, float screenH, Vector3d physOrigin) const;
};
