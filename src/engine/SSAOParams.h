#pragma once

#include "pch/Pch.h"

struct SSAOParams {
    XMMATRIX viewProjection;
    XMFLOAT4 samples[64];
    XMFLOAT3 camPos;
    float radius = 2.0f;
    float bias = 0.05f;
    XMFLOAT2 screenSize;
    float pad1;
    float pad2;
};


