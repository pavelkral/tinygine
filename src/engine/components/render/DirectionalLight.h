#pragma once

#include "pch/Pch.h"

class DirectionalLight : public Component {
public:
    SM::Vector3 color = { 1.0f, 1.0f, 1.0f };
    float intensity = 5.0f;
    bool castShadows = true;    DirectionalLight();    json Serialize() override;    void Deserialize(const json& j) override;    void OnGUI() override;
};

