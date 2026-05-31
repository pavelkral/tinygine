#pragma once

#include "pch/Pch.h"

class PointLight : public Component {
public:
    SM::Vector3 color = { 1.0f, 1.0f, 1.0f };
    float intensity = 10.0f;
    float radius = 10.0f;    PointLight();    json Serialize() override;    void Deserialize(const json& j) override;    void OnGUI() override;
};

// ============================================================================
// --- JOLT PHYSICS CONTACT LISTENER ---
// ============================================================================

