#pragma once

#include "pch/Pch.h"
#include "engine/scene/SceneTypes.h"

struct Transform {
    Vector3d position;
    SM::Vector3 eulerAngles;
    SM::Quaternion rotation;
    SM::Vector3 scale;    
    Transform();    
    void UpdateRotation();
};


